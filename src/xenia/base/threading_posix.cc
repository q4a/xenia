/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2014 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/threading.h"

#include "xenia/base/assert.h"
#include "xenia/base/logging.h"

#include <pthread.h>
#include <signal.h>
#include <sys/eventfd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctime>
#include <memory>

namespace xe {
namespace threading {

template <typename _Rep, typename _Period>
inline timespec DurationToTimeSpec(
    std::chrono::duration<_Rep, _Period> duration) {
  auto nanoseconds =
      std::chrono::duration_cast<std::chrono::nanoseconds>(duration);
  auto div = ldiv(nanoseconds.count(), 1000000000L);
  return timespec{div.quot, div.rem};
}

// Thread interruption is done using user-defined signals
// This implementation uses the SIGRTMAX - SIGRTMIN to signal to a thread
// gdb tip, for SIG = SIGRTMIN + SignalType : handle SIG nostop
// lldb tip, for SIG = SIGRTMIN + SignalType : process handle SIG -s false
enum class SignalType {
  kHighResolutionTimer,
  kTimer,
  kThreadSuspend,
  kThreadUserCallback,
  k_Count
};

int GetSystemSignal(SignalType num) {
  auto result = SIGRTMIN + static_cast<int>(num);
  assert_true(result < SIGRTMAX);
  return result;
}

SignalType GetSystemSignalType(int num) {
  return static_cast<SignalType>(num - SIGRTMIN);
}

thread_local std::array<bool, static_cast<size_t>(SignalType::k_Count)>
    signal_handler_installed = {};

static void signal_handler(int signal, siginfo_t* info, void* context);

void install_signal_handler(SignalType type) {
  if (signal_handler_installed[static_cast<size_t>(type)]) return;
  struct sigaction action {};
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = signal_handler;
  sigemptyset(&action.sa_mask);
  if (sigaction(GetSystemSignal(type), &action, nullptr) == -1)
    signal_handler_installed[static_cast<size_t>(type)] = true;
}

// TODO(dougvj)
void EnableAffinityConfiguration() {}

// uint64_t ticks() { return mach_absolute_time(); }

uint32_t current_thread_system_id() {
  return static_cast<uint32_t>(syscall(SYS_gettid));
}

void set_name(const std::string& name) {
  set_name(static_cast<std::thread::native_handle_type>(pthread_self()), name);
}

void set_name(std::thread::native_handle_type handle, const std::string& name) {
  assert_false(name.length() >= 16);
  if (pthread_setname_np(handle, name.c_str()) != 0) assert_always();
}

void MaybeYield() {
  pthread_yield();
  __sync_synchronize();
}

void SyncMemory() { __sync_synchronize(); }

void Sleep(std::chrono::microseconds duration) {
  timespec rqtp = DurationToTimeSpec(duration);
  timespec rmtp = {};
  auto p_rqtp = &rqtp;
  auto p_rmtp = &rmtp;
  int ret = 0;
  do {
    ret = nanosleep(p_rqtp, p_rmtp);
    // Swap requested for remaining in case of signal interruption
    // in which case, we start sleeping again for the remainder
    std::swap(p_rqtp, p_rmtp);
  } while (ret == -1 && errno == EINTR);
}

// TODO(bwrsandman) Implement by allowing alert interrupts from IO operations
thread_local bool alertable_state_ = false;
SleepResult AlertableSleep(std::chrono::microseconds duration) {
  alertable_state_ = true;
  Sleep(duration);
  alertable_state_ = false;
  return SleepResult::kSuccess;
}

TlsHandle AllocateTlsHandle() {
  auto key = static_cast<pthread_key_t>(-1);
  auto res = pthread_key_create(&key, nullptr);
  assert_zero(res);
  assert_true(key != static_cast<pthread_key_t>(-1));
  return static_cast<TlsHandle>(key);
}

bool FreeTlsHandle(TlsHandle handle) {
  return pthread_key_delete(static_cast<pthread_key_t>(handle)) == 0;
}

uintptr_t GetTlsValue(TlsHandle handle) {
  return reinterpret_cast<uintptr_t>(
      pthread_getspecific(static_cast<pthread_key_t>(handle)));
}

bool SetTlsValue(TlsHandle handle, uintptr_t value) {
  return pthread_setspecific(static_cast<pthread_key_t>(handle),
                             reinterpret_cast<void*>(value)) == 0;
}

class PosixHighResolutionTimer : public HighResolutionTimer {
 public:
  explicit PosixHighResolutionTimer(std::function<void()> callback)
      : callback_(std::move(callback)), timer_(nullptr) {}
  ~PosixHighResolutionTimer() override {
    if (timer_) timer_delete(timer_);
  }

  bool Initialize(std::chrono::milliseconds period) {
    // Create timer
    sigevent sev{};
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = GetSystemSignal(SignalType::kHighResolutionTimer);
    sev.sigev_value.sival_ptr = (void*)&callback_;
    if (timer_create(CLOCK_REALTIME, &sev, &timer_) == -1) return false;

    // Start timer
    itimerspec its{};
    its.it_value = DurationToTimeSpec(period);
    its.it_interval = its.it_value;
    return timer_settime(timer_, 0, &its, nullptr) != -1;
  }

 private:
  std::function<void()> callback_;
  timer_t timer_;
};

std::unique_ptr<HighResolutionTimer> HighResolutionTimer::CreateRepeating(
    std::chrono::milliseconds period, std::function<void()> callback) {
  install_signal_handler(SignalType::kHighResolutionTimer);
  auto timer = std::make_unique<PosixHighResolutionTimer>(std::move(callback));
  if (!timer->Initialize(period)) {
    return nullptr;
  }
  return std::unique_ptr<HighResolutionTimer>(timer.release());
}

class PosixConditionBase {
 public:
  virtual bool Signal() = 0;

  WaitResult Wait(std::chrono::milliseconds timeout) {
    bool executed;
    auto predicate = [this] { return this->signaled(); };
    auto lock = std::unique_lock<std::mutex>(mutex_);
    if (predicate()) {
      executed = true;
    } else {
      if (timeout == std::chrono::milliseconds::max()) {
        cond_.wait(lock, predicate);
        executed = true;  // Did not time out;
      } else {
        executed = cond_.wait_for(lock, timeout, predicate);
      }
    }
    if (executed) {
      post_execution();
      return WaitResult::kSuccess;
    } else {
      return WaitResult::kTimeout;
    }
  }

  static std::pair<WaitResult, size_t> WaitMultiple(
      std::vector<PosixConditionBase*>&& handles, bool wait_all,
      std::chrono::milliseconds timeout) {
    using iter_t = std::vector<PosixConditionBase*>::const_iterator;
    bool executed;
    auto predicate = [](auto h) { return h->signaled(); };

    // Construct a condition for all or any depending on wait_all
    auto operation = wait_all ? std::all_of<iter_t, decltype(predicate)>
                              : std::any_of<iter_t, decltype(predicate)>;
    auto aggregate = [&handles, operation, predicate] {
      return operation(handles.cbegin(), handles.cend(), predicate);
    };

    std::unique_lock<std::mutex> lock(PosixConditionBase::mutex_);

    // Check if the aggregate lambda (all or any) is already satisfied
    if (aggregate()) {
      executed = true;
    } else {
      // If the aggregate is not yet satisfied and the timeout is infinite,
      // wait without timeout.
      if (timeout == std::chrono::milliseconds::max()) {
        PosixConditionBase::cond_.wait(lock, aggregate);
        executed = true;
      } else {
        // Wait with timeout.
        executed = PosixConditionBase::cond_.wait_for(lock, timeout, aggregate);
      }
    }
    if (executed) {
      auto first_signaled = std::numeric_limits<size_t>::max();
      for (auto i = 0u; i < handles.size(); ++i) {
        if (handles[i]->signaled()) {
          if (first_signaled > i) {
            first_signaled = i;
          }
          handles[i]->post_execution();
          if (!wait_all) break;
        }
      }
      return std::make_pair(WaitResult::kSuccess, first_signaled);
    } else {
      return std::make_pair<WaitResult, size_t>(WaitResult::kTimeout, 0);
    }
  }

  virtual void* native_handle() const { return cond_.native_handle(); }

 protected:
  inline virtual bool signaled() const = 0;
  inline virtual void post_execution() = 0;
  static std::condition_variable cond_;
  static std::mutex mutex_;
};

std::condition_variable PosixConditionBase::cond_;
std::mutex PosixConditionBase::mutex_;

// There really is no native POSIX handle for a single wait/signal construct
// pthreads is at a lower level with more handles for such a mechanism.
// This simple wrapper class functions as our handle and uses conditional
// variables for waits and signals.
template <typename T>
class PosixCondition {};

template <>
class PosixCondition<Event> : public PosixConditionBase {
 public:
  PosixCondition(bool manual_reset, bool initial_state)
      : signal_(initial_state), manual_reset_(manual_reset) {}
  virtual ~PosixCondition() = default;

  bool Signal() override {
    auto lock = std::unique_lock<std::mutex>(mutex_);
    signal_ = true;
    if (manual_reset_) {
      cond_.notify_all();
    } else {
      cond_.notify_one();
    }
    return true;
  }

  void Reset() {
    auto lock = std::unique_lock<std::mutex>(mutex_);
    signal_ = false;
  }

 private:
  inline bool signaled() const override { return signal_; }
  inline void post_execution() override {
    if (!manual_reset_) {
      signal_ = false;
    }
  }
  bool signal_;
  const bool manual_reset_;
};

template <>
class PosixCondition<Semaphore> : public PosixConditionBase {
 public:
  PosixCondition(uint32_t initial_count, uint32_t maximum_count)
      : count_(initial_count), maximum_count_(maximum_count) {}

  bool Signal() override { return Release(1, nullptr); }

  bool Release(uint32_t release_count, int* out_previous_count) {
    if (maximum_count_ - count_ >= release_count) {
      auto lock = std::unique_lock<std::mutex>(mutex_);
      if (out_previous_count) *out_previous_count = count_;
      count_ += release_count;
      cond_.notify_all();
      return true;
    }
    return false;
  }

 private:
  inline bool signaled() const override { return count_ > 0; }
  inline void post_execution() override {
    count_--;
    cond_.notify_all();
  }
  uint32_t count_;
  const uint32_t maximum_count_;
};

template <>
class PosixCondition<Mutant> : public PosixConditionBase {
 public:
  explicit PosixCondition(bool initial_owner) : count_(0) {
    if (initial_owner) {
      count_ = 1;
      owner_ = std::this_thread::get_id();
    }
  }

  bool Signal() override { return Release(); }

  bool Release() {
    if (owner_ == std::this_thread::get_id() && count_ > 0) {
      auto lock = std::unique_lock<std::mutex>(mutex_);
      --count_;
      // Free to be acquired by another thread
      if (count_ == 0) {
        cond_.notify_one();
      }
      return true;
    }
    return false;
  }

  void* native_handle() const override { return mutex_.native_handle(); }

 private:
  inline bool signaled() const override {
    return count_ == 0 || owner_ == std::this_thread::get_id();
  }
  inline void post_execution() override {
    count_++;
    owner_ = std::this_thread::get_id();
  }
  uint32_t count_;
  std::thread::id owner_;
};

template <>
class PosixCondition<Timer> : public PosixConditionBase {
 public:
  explicit PosixCondition(bool manual_reset)
      : callback_(),
        timer_(nullptr),
        signal_(false),
        manual_reset_(manual_reset) {}

  virtual ~PosixCondition() { Cancel(); }

  bool Signal() override {
    CompletionRoutine();
    return true;
  }

  // TODO(bwrsandman): due_times of under 1ms deadlock under travis
  bool Set(std::chrono::nanoseconds due_time, std::chrono::milliseconds period,
           std::function<void()> opt_callback = nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);

    callback_ = std::move(opt_callback);
    signal_ = false;

    // Create timer
    if (timer_ == nullptr) {
      sigevent sev{};
      sev.sigev_notify = SIGEV_SIGNAL;
      sev.sigev_signo = GetSystemSignal(SignalType::kTimer);
      sev.sigev_value.sival_ptr = this;
      if (timer_create(CLOCK_REALTIME, &sev, &timer_) == -1) return false;
    }

    // Start timer
    itimerspec its{};
    its.it_value = DurationToTimeSpec(due_time);
    its.it_interval = DurationToTimeSpec(period);
    return timer_settime(timer_, 0, &its, nullptr) == 0;
  }

  void CompletionRoutine() {
    // As the callback may reset the timer, store local.
    std::function<void()> callback;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      // Store callback
      if (callback_) callback = callback_;
      signal_ = true;
      if (manual_reset_) {
        cond_.notify_all();
      } else {
        cond_.notify_one();
      }
    }
    // Call callback
    if (callback) callback();
  }

  bool Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    bool result = true;
    if (timer_) {
      result = timer_delete(timer_) == 0;
      timer_ = nullptr;
    }
    return result;
  }

  void* native_handle() const override {
    return reinterpret_cast<void*>(timer_);
  }

 private:
  inline bool signaled() const override { return signal_; }
  inline void post_execution() override {
    if (!manual_reset_) {
      signal_ = false;
    }
  }
  std::function<void()> callback_;
  timer_t timer_;
  volatile bool signal_;
  const bool manual_reset_;
};

struct ThreadStartData {
  std::function<void()> start_routine;
  bool create_suspended;
  Thread* thread_obj;
};

template <>
class PosixCondition<Thread> : public PosixConditionBase {
  enum class State {
    kUninitialized,
    kRunning,
    kSuspended,
    kFinished,
  };

 public:
  PosixCondition()
      : thread_(0),
        signaled_(false),
        exit_code_(0),
        state_(State::kUninitialized),
        suspend_count_(0) {}
  bool Initialize(Thread::CreationParameters params,
                  ThreadStartData* start_data) {
    start_data->create_suspended = params.create_suspended;
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) return false;
    if (pthread_attr_setstacksize(&attr, params.stack_size) != 0) {
      pthread_attr_destroy(&attr);
      return false;
    }
    if (params.initial_priority != 0) {
      sched_param sched{};
      sched.sched_priority = params.initial_priority + 1;
      if (pthread_attr_setschedpolicy(&attr, SCHED_FIFO) != 0) {
        pthread_attr_destroy(&attr);
        return false;
      }
      if (pthread_attr_setschedparam(&attr, &sched) != 0) {
        pthread_attr_destroy(&attr);
        return false;
      }
    }
    if (pthread_create(&thread_, &attr, ThreadStartRoutine, start_data) != 0) {
      return false;
    }
    pthread_attr_destroy(&attr);
    return true;
  }

  /// Constructor for existing thread. This should only happen once called by
  /// Thread::GetCurrentThread() on the main thread
  explicit PosixCondition(pthread_t thread)
      : thread_(thread),
        signaled_(false),
        exit_code_(0),
        state_(State::kRunning) {}

  virtual ~PosixCondition() {
    if (thread_ && !signaled_) {
      if (pthread_cancel(thread_) != 0) {
        assert_always();
      }
      if (pthread_join(thread_, nullptr) != 0) {
        assert_always();
      }
    }
  }

  bool Signal() override { return true; }

  std::string name() const {
    WaitStarted();
    auto result = std::array<char, 17>{'\0'};
    std::unique_lock<std::mutex> lock(state_mutex_);
    if (state_ != State::kUninitialized && state_ != State::kFinished) {
      if (pthread_getname_np(thread_, result.data(), result.size() - 1) != 0)
        assert_always();
    }
    return std::string(result.data());
  }

  void set_name(const std::string& name) {
    WaitStarted();
    std::unique_lock<std::mutex> lock(state_mutex_);
    if (state_ != State::kUninitialized && state_ != State::kFinished) {
      threading::set_name(static_cast<std::thread::native_handle_type>(thread_),
                          name);
    }
  }

  uint32_t system_id() const { return static_cast<uint32_t>(thread_); }

  uint64_t affinity_mask() {
    WaitStarted();
    cpu_set_t cpu_set;
    if (pthread_getaffinity_np(thread_, sizeof(cpu_set_t), &cpu_set) != 0)
      assert_always();
    uint64_t result = 0;
    auto cpu_count = std::min(CPU_SETSIZE, 64);
    for (auto i = 0u; i < cpu_count; i++) {
      auto set = CPU_ISSET(i, &cpu_set);
      result |= set << i;
    }
    return result;
  }

  void set_affinity_mask(uint64_t mask) {
    WaitStarted();
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    for (auto i = 0u; i < 64; i++) {
      if (mask & (1 << i)) {
        CPU_SET(i, &cpu_set);
      }
    }
    if (pthread_setaffinity_np(thread_, sizeof(cpu_set_t), &cpu_set) != 0) {
      assert_always();
    }
  }

  int priority() {
    WaitStarted();
    int policy;
    sched_param param{};
    int ret = pthread_getschedparam(thread_, &policy, &param);
    if (ret != 0) {
      return -1;
    }

    return param.sched_priority;
  }

  void set_priority(int new_priority) {
    WaitStarted();
    sched_param param{};
    param.sched_priority = new_priority;
    if (pthread_setschedparam(thread_, SCHED_FIFO, &param) != 0)
      assert_always();
  }

  void QueueUserCallback(std::function<void()> callback) {
    WaitStarted();
    std::unique_lock<std::mutex> lock(callback_mutex_);
    user_callback_ = std::move(callback);
    sigval value{};
    value.sival_ptr = this;
    pthread_sigqueue(thread_, GetSystemSignal(SignalType::kThreadUserCallback),
                     value);
  }

  void CallUserCallback() {
    std::unique_lock<std::mutex> lock(callback_mutex_);
    user_callback_();
  }

  bool Resume(uint32_t* out_previous_suspend_count = nullptr) {
    if (out_previous_suspend_count) {
      *out_previous_suspend_count = 0;
    }
    WaitStarted();
    std::unique_lock<std::mutex> lock(state_mutex_);
    if (state_ != State::kSuspended) return false;
    if (out_previous_suspend_count) {
      *out_previous_suspend_count = suspend_count_;
    }
    --suspend_count_;
    state_signal_.notify_all();
    return true;
  }

  bool Suspend(uint32_t* out_previous_suspend_count = nullptr) {
    if (out_previous_suspend_count) {
      *out_previous_suspend_count = 0;
    }
    WaitStarted();
    {
      if (out_previous_suspend_count) {
        *out_previous_suspend_count = suspend_count_;
      }
      state_ = State::kSuspended;
      ++suspend_count_;
    }
    int result =
        pthread_kill(thread_, GetSystemSignal(SignalType::kThreadSuspend));
    return result == 0;
  }

  void Terminate(int exit_code) {
    {
      std::unique_lock<std::mutex> lock(state_mutex_);
      state_ = State::kFinished;
    }

    std::lock_guard<std::mutex> lock(mutex_);

    // Sometimes the thread can call terminate twice before stopping
    if (thread_ == 0) return;
    auto thread = thread_;

    exit_code_ = exit_code;
    signaled_ = true;
    cond_.notify_all();

    if (pthread_cancel(thread) != 0) assert_always();
  }

  void WaitStarted() const {
    std::unique_lock<std::mutex> lock(state_mutex_);
    state_signal_.wait(lock,
                       [this] { return state_ != State::kUninitialized; });
  }

  /// Set state to suspended and wait until it reset by another thread
  void WaitSuspended() {
    std::unique_lock<std::mutex> lock(state_mutex_);
    state_signal_.wait(lock, [this] { return suspend_count_ == 0; });
    state_ = State::kRunning;
  }

  void* native_handle() const override {
    return reinterpret_cast<void*>(thread_);
  }

 private:
  static void* ThreadStartRoutine(void* parameter);
  inline bool signaled() const override { return signaled_; }
  inline void post_execution() override {
    if (thread_) {
      pthread_join(thread_, nullptr);
      thread_ = 0;
    }
  }
  pthread_t thread_;
  bool signaled_;
  int exit_code_;
  volatile State state_;
  volatile uint32_t suspend_count_;
  mutable std::mutex state_mutex_;
  mutable std::mutex callback_mutex_;
  mutable std::condition_variable state_signal_;
  std::function<void()> user_callback_;
};

class PosixWaitHandle {
 public:
  virtual PosixConditionBase& condition() = 0;
};

// This wraps a condition object as our handle because posix has no single
// native handle for higher level concurrency constructs such as semaphores
template <typename T>
class PosixConditionHandle : public T, public PosixWaitHandle {
 public:
  PosixConditionHandle() = default;
  explicit PosixConditionHandle(bool);
  explicit PosixConditionHandle(pthread_t thread);
  PosixConditionHandle(bool manual_reset, bool initial_state);
  PosixConditionHandle(uint32_t initial_count, uint32_t maximum_count);
  ~PosixConditionHandle() override = default;

  PosixConditionBase& condition() override { return handle_; }
  void* native_handle() const override { return handle_.native_handle(); }

 protected:
  PosixCondition<T> handle_;
  friend PosixCondition<T>;
};

template <>
PosixConditionHandle<Semaphore>::PosixConditionHandle(uint32_t initial_count,
                                                      uint32_t maximum_count)
    : handle_(initial_count, maximum_count) {}

template <>
PosixConditionHandle<Mutant>::PosixConditionHandle(bool initial_owner)
    : handle_(initial_owner) {}

template <>
PosixConditionHandle<Timer>::PosixConditionHandle(bool manual_reset)
    : handle_(manual_reset) {}

template <>
PosixConditionHandle<Event>::PosixConditionHandle(bool manual_reset,
                                                  bool initial_state)
    : handle_(manual_reset, initial_state) {}

template <>
PosixConditionHandle<Thread>::PosixConditionHandle(pthread_t thread)
    : handle_(thread) {}

WaitResult Wait(WaitHandle* wait_handle, bool is_alertable,
                std::chrono::milliseconds timeout) {
  auto posix_wait_handle = dynamic_cast<PosixWaitHandle*>(wait_handle);
  if (posix_wait_handle == nullptr) {
    return WaitResult::kFailed;
  }
  if (is_alertable) alertable_state_ = true;
  auto result = posix_wait_handle->condition().Wait(timeout);
  if (is_alertable) alertable_state_ = false;
  return result;
}

WaitResult SignalAndWait(WaitHandle* wait_handle_to_signal,
                         WaitHandle* wait_handle_to_wait_on, bool is_alertable,
                         std::chrono::milliseconds timeout) {
  auto result = WaitResult::kFailed;
  auto posix_wait_handle_to_signal =
      dynamic_cast<PosixWaitHandle*>(wait_handle_to_signal);
  auto posix_wait_handle_to_wait_on =
      dynamic_cast<PosixWaitHandle*>(wait_handle_to_wait_on);
  if (posix_wait_handle_to_signal == nullptr ||
      posix_wait_handle_to_wait_on == nullptr) {
    return WaitResult::kFailed;
  }
  if (is_alertable) alertable_state_ = true;
  if (posix_wait_handle_to_signal->condition().Signal()) {
    result = posix_wait_handle_to_wait_on->condition().Wait(timeout);
  }
  if (is_alertable) alertable_state_ = false;
  return result;
}

std::pair<WaitResult, size_t> WaitMultiple(WaitHandle* wait_handles[],
                                           size_t wait_handle_count,
                                           bool wait_all, bool is_alertable,
                                           std::chrono::milliseconds timeout) {
  std::vector<PosixConditionBase*> conditions;
  conditions.reserve(wait_handle_count);
  for (size_t i = 0u; i < wait_handle_count; ++i) {
    auto handle = dynamic_cast<PosixWaitHandle*>(wait_handles[i]);
    if (handle == nullptr) {
      return std::make_pair(WaitResult::kFailed, 0);
    }
    conditions.push_back(&handle->condition());
  }
  if (is_alertable) alertable_state_ = true;
  auto result = PosixConditionBase::WaitMultiple(std::move(conditions),
                                                 wait_all, timeout);
  if (is_alertable) alertable_state_ = false;
  return result;
}

class PosixEvent : public PosixConditionHandle<Event> {
 public:
  PosixEvent(bool manual_reset, bool initial_state)
      : PosixConditionHandle(manual_reset, initial_state) {}
  ~PosixEvent() override = default;
  void Set() override { handle_.Signal(); }
  void Reset() override { handle_.Reset(); }
  void Pulse() override {
    using namespace std::chrono_literals;
    handle_.Signal();
    MaybeYield();
    Sleep(10us);
    handle_.Reset();
  }
};

std::unique_ptr<Event> Event::CreateManualResetEvent(bool initial_state) {
  return std::make_unique<PosixEvent>(true, initial_state);
}

std::unique_ptr<Event> Event::CreateAutoResetEvent(bool initial_state) {
  return std::make_unique<PosixEvent>(false, initial_state);
}

class PosixSemaphore : public PosixConditionHandle<Semaphore> {
 public:
  PosixSemaphore(int initial_count, int maximum_count)
      : PosixConditionHandle(static_cast<uint32_t>(initial_count),
                             static_cast<uint32_t>(maximum_count)) {}
  ~PosixSemaphore() override = default;
  bool Release(int release_count, int* out_previous_count) override {
    if (release_count < 1) {
      return false;
    }
    return handle_.Release(static_cast<uint32_t>(release_count),
                           out_previous_count);
  }
};

std::unique_ptr<Semaphore> Semaphore::Create(int initial_count,
                                             int maximum_count) {
  return std::make_unique<PosixSemaphore>(initial_count, maximum_count);
}

class PosixMutant : public PosixConditionHandle<Mutant> {
 public:
  explicit PosixMutant(bool initial_owner)
      : PosixConditionHandle(initial_owner) {}
  ~PosixMutant() override = default;
  bool Release() override { return handle_.Release(); }
};

std::unique_ptr<Mutant> Mutant::Create(bool initial_owner) {
  return std::make_unique<PosixMutant>(initial_owner);
}

class PosixTimer : public PosixConditionHandle<Timer> {
 public:
  explicit PosixTimer(bool manual_reset) : PosixConditionHandle(manual_reset) {}
  ~PosixTimer() override = default;
  bool SetOnce(std::chrono::nanoseconds due_time,
               std::function<void()> opt_callback) override {
    return handle_.Set(due_time, std::chrono::milliseconds::zero(),
                       std::move(opt_callback));
  }
  bool SetRepeating(std::chrono::nanoseconds due_time,
                    std::chrono::milliseconds period,
                    std::function<void()> opt_callback) override {
    return handle_.Set(due_time, period, std::move(opt_callback));
  }
  bool Cancel() override { return handle_.Cancel(); }
};

std::unique_ptr<Timer> Timer::CreateManualResetTimer() {
  install_signal_handler(SignalType::kTimer);
  return std::make_unique<PosixTimer>(true);
}

std::unique_ptr<Timer> Timer::CreateSynchronizationTimer() {
  install_signal_handler(SignalType::kTimer);
  return std::make_unique<PosixTimer>(false);
}

class PosixThread : public PosixConditionHandle<Thread> {
 public:
  PosixThread() = default;
  explicit PosixThread(pthread_t thread) : PosixConditionHandle(thread) {}
  ~PosixThread() override = default;

  bool Initialize(CreationParameters params,
                  std::function<void()> start_routine) {
    auto start_data =
        new ThreadStartData({std::move(start_routine), false, this});
    return handle_.Initialize(params, start_data);
  }

  std::string name() const override {
    handle_.WaitStarted();
    auto result = Thread::name();
    if (result.empty()) {
      result = handle_.name();
    }
    return result;
  }

  void set_name(std::string name) override {
    handle_.WaitStarted();
    Thread::set_name(name);
    if (name.length() > 15) {
      name = name.substr(0, 15);
    }
    handle_.set_name(name);
  }

  uint32_t system_id() const override { return handle_.system_id(); }

  uint64_t affinity_mask() override { return handle_.affinity_mask(); }
  void set_affinity_mask(uint64_t mask) override {
    handle_.set_affinity_mask(mask);
  }

  int priority() override { return handle_.priority(); }
  void set_priority(int new_priority) override {
    handle_.set_priority(new_priority);
  }

  void QueueUserCallback(std::function<void()> callback) override {
    handle_.QueueUserCallback(std::move(callback));
  }

  bool Resume(uint32_t* out_previous_suspend_count) override {
    return handle_.Resume(out_previous_suspend_count);
  }

  bool Suspend(uint32_t* out_previous_suspend_count) override {
    return handle_.Suspend(out_previous_suspend_count);
  }

  void Terminate(int exit_code) override { handle_.Terminate(exit_code); }

  void WaitSuspended() { handle_.WaitSuspended(); }
};

thread_local PosixThread* current_thread_ = nullptr;

void* PosixCondition<Thread>::ThreadStartRoutine(void* parameter) {
  if (pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, nullptr) != 0) {
    assert_always();
  }
  threading::set_name("");

  auto start_data = static_cast<ThreadStartData*>(parameter);
  assert_not_null(start_data);
  assert_not_null(start_data->thread_obj);

  auto thread = dynamic_cast<PosixThread*>(start_data->thread_obj);
  auto start_routine = std::move(start_data->start_routine);
  auto create_suspended = start_data->create_suspended;
  delete start_data;

  current_thread_ = thread;
  {
    std::unique_lock<std::mutex> lock(thread->handle_.state_mutex_);
    thread->handle_.state_ =
        create_suspended ? State::kSuspended : State::kRunning;
    thread->handle_.state_signal_.notify_all();
  }

  if (create_suspended) {
    std::unique_lock<std::mutex> lock(thread->handle_.state_mutex_);
    thread->handle_.suspend_count_ = 1;
    thread->handle_.state_signal_.wait(
        lock, [thread] { return thread->handle_.suspend_count_ == 0; });
  }

  start_routine();

  {
    std::unique_lock<std::mutex> lock(thread->handle_.state_mutex_);
    thread->handle_.state_ = State::kFinished;
  }

  std::unique_lock<std::mutex> lock(mutex_);
  thread->handle_.exit_code_ = 0;
  thread->handle_.signaled_ = true;
  cond_.notify_all();

  current_thread_ = nullptr;
  return nullptr;
}

std::unique_ptr<Thread> Thread::Create(CreationParameters params,
                                       std::function<void()> start_routine) {
  install_signal_handler(SignalType::kThreadSuspend);
  install_signal_handler(SignalType::kThreadUserCallback);
  auto thread = std::make_unique<PosixThread>();
  if (!thread->Initialize(params, std::move(start_routine))) return nullptr;
  assert_not_null(thread);
  return thread;
}

Thread* Thread::GetCurrentThread() {
  if (current_thread_) {
    return current_thread_;
  }

  // Should take this route only for threads not created by Thread::Create.
  // The only thread not created by Thread::Create should be the main thread.
  pthread_t handle = pthread_self();

  current_thread_ = new PosixThread(handle);
  atexit([] { delete current_thread_; });

  return current_thread_;
}

void Thread::Exit(int exit_code) {
  if (current_thread_) {
    current_thread_->Terminate(exit_code);
    // Sometimes the current thread keeps running after being cancelled.
    // Prevent other calls from this thread from using current_thread_.
    current_thread_ = nullptr;
  } else {
    // Should only happen with the main thread
    pthread_exit(reinterpret_cast<void*>(exit_code));
  }
}

static void signal_handler(int signal, siginfo_t* info, void* /*context*/) {
  switch (GetSystemSignalType(signal)) {
    case SignalType::kHighResolutionTimer: {
      assert_not_null(info->si_value.sival_ptr);
      auto callback =
          *static_cast<std::function<void()>*>(info->si_value.sival_ptr);
      callback();
    } break;
    case SignalType::kTimer: {
      assert_not_null(info->si_value.sival_ptr);
      auto pTimer =
          static_cast<PosixCondition<Timer>*>(info->si_value.sival_ptr);
      pTimer->CompletionRoutine();
    } break;
    case SignalType::kThreadSuspend: {
      assert_not_null(current_thread_);
      current_thread_->WaitSuspended();
    } break;
    case SignalType::kThreadUserCallback: {
      assert_not_null(info->si_value.sival_ptr);
      auto p_thread =
          static_cast<PosixCondition<Thread>*>(info->si_value.sival_ptr);
      if (alertable_state_) {
        p_thread->CallUserCallback();
      }
    } break;
    default:
      assert_always();
  }
}

}  // namespace threading
}  // namespace xe
