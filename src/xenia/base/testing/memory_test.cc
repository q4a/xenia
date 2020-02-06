/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2015 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#include "xenia/base/memory.h"

#include "third_party/catch/include/catch.hpp"

namespace xe {
namespace base {
namespace test {

TEST_CASE("copy_128_aligned", "Copy and Swap") {
  alignas(128) uint8_t src[256], dest[256];
  for (uint8_t i = 0; i < 255; ++i) {
    src[i] = 255 - i;
  }
  std::memset(dest, 0, sizeof(dest));
  copy_128_aligned(dest, src, 1);
  REQUIRE(std::memcmp(dest, src, 128));
  REQUIRE(dest[128] == 0);

  std::memset(dest, 0, sizeof(dest));
  copy_128_aligned(dest, src, 2);
  REQUIRE(std::memcmp(dest, src, 256));

  std::memset(dest, 0, sizeof(dest));
  copy_128_aligned(dest, src + 1, 1);
  REQUIRE(std::memcmp(dest, src + 1, 128));
}

TEST_CASE("copy_and_swap_16_aligned", "Copy and Swap") {
  alignas(16) uint16_t a = 0x1111, b = 0xABCD;
  copy_and_swap_16_aligned(&a, &b, 1);
  REQUIRE(a == 0xCDAB);
  REQUIRE(b == 0xABCD);

  alignas(16) uint16_t c[] = {0x0000, 0x0000, 0x0000, 0x0000};
  alignas(16) uint16_t d[] = {0x0123, 0x4567, 0x89AB, 0xCDEF};
  copy_and_swap_16_aligned(c, d, 1);
  REQUIRE(c[0] == 0x2301);
  REQUIRE(c[1] == 0x0000);
  REQUIRE(c[2] == 0x0000);
  REQUIRE(c[3] == 0x0000);

  copy_and_swap_16_aligned(c, d, 3);
  REQUIRE(c[0] == 0x2301);
  REQUIRE(c[1] == 0x6745);
  REQUIRE(c[2] == 0xAB89);
  REQUIRE(c[3] == 0x0000);

  copy_and_swap_16_aligned(c, d, 4);
  REQUIRE(c[0] == 0x2301);
  REQUIRE(c[1] == 0x6745);
  REQUIRE(c[2] == 0xAB89);
  REQUIRE(c[3] == 0xEFCD);

  alignas(16) uint64_t e;
  copy_and_swap_16_aligned(&e, d, 4);
  REQUIRE(e == 0xEFCDAB8967452301);

  alignas(16) char f[85] = {0x00};
  alignas(16) char g[] =
      "This is a 85 byte long string... "
      "It's supposed to be longer than standard alignment.";
  copy_and_swap_16_aligned(f, g, 42);
  REQUIRE(std::strcmp(f,
                      "hTsii  s a58b ty eolgns rtni.g..I 't susppsodet  oebl "
                      "noeg rhtnas atdnra dlagimnne.t") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_16_aligned(f, g + 16, 34);
  REQUIRE(std::strcmp(f,
                      " eolgns rtni.g..I 't susppsodet  oebl "
                      "noeg rhtnas atdnra dlagimnne.t") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_16_aligned(f, g + 32, 26);
  REQUIRE(std::strcmp(f,
                      "I 't susppsodet  oebl "
                      "noeg rhtnas atdnra dlagimnne.t") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_16_aligned(f, g + 64, 10);
  REQUIRE(std::strcmp(f, "s atdnra dlagimnne.t") == 0);
}

TEST_CASE("copy_and_swap_16_unaligned", "Copy and Swap") {
  uint16_t a = 0x1111, b = 0xABCD;
  copy_and_swap_16_unaligned(&a, &b, 1);
  REQUIRE(a == 0xCDAB);
  REQUIRE(b == 0xABCD);

  uint16_t c[] = {0x0000, 0x0000, 0x0000, 0x0000};
  uint16_t d[] = {0x0123, 0x4567, 0x89AB, 0xCDEF};
  copy_and_swap_16_unaligned(c, d, 1);
  REQUIRE(c[0] == 0x2301);
  REQUIRE(c[1] == 0x0000);
  REQUIRE(c[2] == 0x0000);
  REQUIRE(c[3] == 0x0000);

  copy_and_swap_16_unaligned(c, d, 4);
  REQUIRE(c[0] == 0x2301);
  REQUIRE(c[1] == 0x6745);
  REQUIRE(c[2] == 0xAB89);
  REQUIRE(c[3] == 0xEFCD);

  uint64_t e;
  copy_and_swap_16_unaligned(&e, d, 4);
  REQUIRE(e == 0xEFCDAB8967452301);

  char f[85] = {0x00};
  char g[] =
      "This is a 85 byte long string... "
      "It's supposed to be longer than standard alignment.";
  copy_and_swap_16_unaligned(f, g, 42);
  REQUIRE(std::strcmp(f,
                      "hTsii  s a58b ty eolgns rtni.g..I 't susppsodet  oebl "
                      "noeg rhtnas atdnra dlagimnne.t") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_16_unaligned(f, g + 1, 41);
  REQUIRE(std::strcmp(f,
                      "ih ssia 8  5ybetl no gtsirgn.. .tIs's puopes dotb  "
                      "eolgnret ah ntsnaaddra ilngemtn") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_16_unaligned(f, g + 2, 41);
  REQUIRE(std::strcmp(f,
                      "sii  s a58b ty eolgns rtni.g..I 't susppsodet  oebl "
                      "noeg rhtnas atdnra dlagimnne.t") == 0);
}

TEST_CASE("copy_and_swap_32_aligned", "Copy and Swap") {
  alignas(32) uint32_t a = 0x11111111, b = 0x89ABCDEF;
  copy_and_swap_32_aligned(&a, &b, 1);
  REQUIRE(a == 0xEFCDAB89);
  REQUIRE(b == 0x89ABCDEF);

  alignas(32) uint32_t c[] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
  alignas(32) uint32_t d[] = {0x01234567, 0x89ABCDEF, 0xE887EEED, 0xD8514199};
  copy_and_swap_32_aligned(c, d, 1);
  REQUIRE(c[0] == 0x67452301);
  REQUIRE(c[1] == 0x00000000);
  REQUIRE(c[2] == 0x00000000);
  REQUIRE(c[3] == 0x00000000);

  copy_and_swap_32_aligned(c, d, 3);
  REQUIRE(c[0] == 0x67452301);
  REQUIRE(c[1] == 0xEFCDAB89);
  REQUIRE(c[2] == 0xEDEE87E8);
  REQUIRE(c[3] == 0x00000000);

  copy_and_swap_32_aligned(c, d, 4);
  REQUIRE(c[0] == 0x67452301);
  REQUIRE(c[1] == 0xEFCDAB89);
  REQUIRE(c[2] == 0xEDEE87E8);
  REQUIRE(c[3] == 0x994151D8);

  alignas(32) uint64_t e;
  copy_and_swap_32_aligned(&e, d, 2);
  REQUIRE(e == 0xEFCDAB8967452301);

  alignas(32) char f[85] = {0x00};
  alignas(32) char g[] =
      "This is a 85 byte long string... "
      "It's supposed to be longer than standard alignment.";
  copy_and_swap_32_aligned(f, g, 21);
  REQUIRE(std::strcmp(f,
                      "sihT si 58 atyb ol es gnnirt...g'tI us ssoppt deeb "
                      "onol  regnahtats radnla dmngi.tne") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_32_aligned(f, g + 16, 17);
  REQUIRE(std::strcmp(f,
                      "ol es gnnirt...g'tI us ssoppt deeb "
                      "onol  regnahtats radnla dmngi.tne") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_32_aligned(f, g + 32, 13);
  REQUIRE(std::strcmp(f,
                      "'tI us ssoppt deeb "
                      "onol  regnahtats radnla dmngi.tne") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_32_aligned(f, g + 64, 5);
  REQUIRE(std::strcmp(f, "ats radnla dmngi.tne") == 0);
}

TEST_CASE("copy_and_swap_32_unaligned", "Copy and Swap") {
  uint32_t a = 0x11111111, b = 0x89ABCDEF;
  copy_and_swap_32_unaligned(&a, &b, 1);
  REQUIRE(a == 0xEFCDAB89);
  REQUIRE(b == 0x89ABCDEF);

  uint32_t c[] = {0x00000000, 0x00000000, 0x00000000, 0x00000000};
  uint32_t d[] = {0x01234567, 0x89ABCDEF, 0xE887EEED, 0xD8514199};
  copy_and_swap_32_unaligned(c, d, 1);
  REQUIRE(c[0] == 0x67452301);
  REQUIRE(c[1] == 0x00000000);
  REQUIRE(c[2] == 0x00000000);
  REQUIRE(c[3] == 0x00000000);

  copy_and_swap_32_unaligned(c, d, 3);
  REQUIRE(c[0] == 0x67452301);
  REQUIRE(c[1] == 0xEFCDAB89);
  REQUIRE(c[2] == 0xEDEE87E8);
  REQUIRE(c[3] == 0x00000000);

  copy_and_swap_32_unaligned(c, d, 4);
  REQUIRE(c[0] == 0x67452301);
  REQUIRE(c[1] == 0xEFCDAB89);
  REQUIRE(c[2] == 0xEDEE87E8);
  REQUIRE(c[3] == 0x994151D8);

  uint64_t e;
  copy_and_swap_32_unaligned(&e, d, 2);
  REQUIRE(e == 0xEFCDAB8967452301);

  char f[85] = {0x00};
  char g[] =
      "This is a 85 byte long string... "
      "It's supposed to be longer than standard alignment.";
  copy_and_swap_32_unaligned(f, g, 21);
  REQUIRE(std::strcmp(f,
                      "sihT si 58 atyb ol es gnnirt...g'tI us ssoppt deeb "
                      "onol  regnahtats radnla dmngi.tne") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_32_unaligned(f, g + 1, 20);
  REQUIRE(std::strcmp(f,
                      " siha si 58 etybnol ts ggnir ...s'tIpus esopot d eb "
                      "gnolt re nahnatsdradila emng") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_32_unaligned(f, g + 2, 20);
  REQUIRE(std::strcmp(f,
                      "i si a sb 58 etygnolrts .gniI .. s'tppusdeso ot l "
                      "ebegnoht rs nadnat dragilanemn") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_32_unaligned(f, g + 3, 20);
  REQUIRE(std::strcmp(f,
                      "si s8 a yb 5l et gnoirts..gntI .s s'oppu desb otol "
                      "eregnaht ts nadnaa drngiltnem") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_32_unaligned(f, g + 4, 20);
  REQUIRE(std::strcmp(f,
                      " si 58 atyb ol es gnnirt...g'tI us ssoppt deeb onol  "
                      "regnahtats radnla dmngi.tne") == 0);
}

TEST_CASE("copy_and_swap_64_aligned", "Copy and Swap") {
  alignas(64) uint64_t a = 0x1111111111111111, b = 0x0123456789ABCDEF;
  copy_and_swap_64_aligned(&a, &b, 1);
  REQUIRE(a == 0xEFCDAB8967452301);
  REQUIRE(b == 0x0123456789ABCDEF);

  alignas(64) uint64_t c[] = {0x0000000000000000, 0x0000000000000000,
                              0x0000000000000000, 0x0000000000000000};
  alignas(64) uint64_t d[] = {0x0123456789ABCDEF, 0xE887EEEDD8514199,
                              0x21D4745A1D4A7706, 0xA4174FED675766E3};
  copy_and_swap_64_aligned(c, d, 1);
  REQUIRE(c[0] == 0xEFCDAB8967452301);
  REQUIRE(c[1] == 0x0000000000000000);
  REQUIRE(c[2] == 0x0000000000000000);
  REQUIRE(c[3] == 0x0000000000000000);

  copy_and_swap_64_aligned(c, d, 3);
  REQUIRE(c[0] == 0xEFCDAB8967452301);
  REQUIRE(c[1] == 0x994151D8EDEE87E8);
  REQUIRE(c[2] == 0x06774A1D5A74D421);
  REQUIRE(c[3] == 0x0000000000000000);

  copy_and_swap_64_aligned(c, d, 4);
  REQUIRE(c[0] == 0xEFCDAB8967452301);
  REQUIRE(c[1] == 0x994151D8EDEE87E8);
  REQUIRE(c[2] == 0x06774A1D5A74D421);
  REQUIRE(c[3] == 0xE3665767ED4F17A4);

  alignas(64) uint64_t e;
  copy_and_swap_64_aligned(&e, d, 1);
  REQUIRE(e == 0xEFCDAB8967452301);

  alignas(64) char f[85] = {0x00};
  alignas(64) char g[] =
      "This is a 85 byte long string... "
      "It's supposed to be longer than standard alignment.";
  copy_and_swap_64_aligned(f, g, 10);
  REQUIRE(std::strcmp(f,
                      " si sihTtyb 58 as gnol e...gnirtus s'tI t desoppnol eb "
                      "onaht regradnats mngila d") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_aligned(f, g + 16, 8);
  REQUIRE(std::strcmp(f,
                      "s gnol e...gnirtus s'tI t desoppnol eb "
                      "onaht regradnats mngila d") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_aligned(f, g + 32, 6);
  REQUIRE(std::strcmp(f,
                      "us s'tI t desoppnol eb "
                      "onaht regradnats mngila d") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_aligned(f, g + 64, 2);
  REQUIRE(std::strcmp(f, "radnats mngila d") == 0);
}

TEST_CASE("copy_and_swap_64_unaligned", "Copy and Swap") {
  uint64_t a = 0x1111111111111111, b = 0x0123456789ABCDEF;
  copy_and_swap_64_unaligned(&a, &b, 1);
  REQUIRE(a == 0xEFCDAB8967452301);
  REQUIRE(b == 0x0123456789ABCDEF);

  uint64_t c[] = {0x0000000000000000, 0x0000000000000000, 0x0000000000000000,
                  0x0000000000000000};
  uint64_t d[] = {0x0123456789ABCDEF, 0xE887EEEDD8514199, 0x21D4745A1D4A7706,
                  0xA4174FED675766E3};
  copy_and_swap_64_unaligned(c, d, 1);
  REQUIRE(c[0] == 0xEFCDAB8967452301);
  REQUIRE(c[1] == 0x0000000000000000);
  REQUIRE(c[2] == 0x0000000000000000);
  REQUIRE(c[3] == 0x0000000000000000);

  copy_and_swap_64_unaligned(c, d, 3);
  REQUIRE(c[0] == 0xEFCDAB8967452301);
  REQUIRE(c[1] == 0x994151D8EDEE87E8);
  REQUIRE(c[2] == 0x06774A1D5A74D421);
  REQUIRE(c[3] == 0x0000000000000000);

  copy_and_swap_64_unaligned(c, d, 4);
  REQUIRE(c[0] == 0xEFCDAB8967452301);
  REQUIRE(c[1] == 0x994151D8EDEE87E8);
  REQUIRE(c[2] == 0x06774A1D5A74D421);
  REQUIRE(c[3] == 0xE3665767ED4F17A4);

  uint64_t e;
  copy_and_swap_64_unaligned(&e, d, 1);
  REQUIRE(e == 0xEFCDAB8967452301);

  char f[85] = {0x00};
  char g[] =
      "This is a 85 byte long string... "
      "It's supposed to be longer than standard alignment.";
  copy_and_swap_64_unaligned(f, g, 10);
  REQUIRE(std::strcmp(f,
                      " si sihTtyb 58 as gnol e...gnirtus s'tI t desoppnol eb "
                      "onaht regradnats mngila d") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_unaligned(f, g + 1, 10);
  REQUIRE(std::strcmp(f,
                      "a si sihetyb 58 ts gnol  ...gnirpus s'tIot desopgnol "
                      "eb  naht redradnatsemngila ") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_unaligned(f, g + 2, 10);
  REQUIRE(std::strcmp(f,
                      " a si si etyb 58rts gnolI ...gnippus s't ot desoegnol "
                      "ebs naht r dradnatnemngila") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_unaligned(f, g + 3, 10);
  REQUIRE(std::strcmp(f,
                      "8 a si sl etyb 5irts gnotI ...gnoppus s'b ot desregnol "
                      "ets naht a dradnatnemngil") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_unaligned(f, g + 4, 10);
  REQUIRE(std::strcmp(f,
                      "58 a si ol etyb nirts gn'tI ...gsoppus seb ot de "
                      "regnol ats nahtla dradn.tnemngi") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_unaligned(f, g + 5, 9);
  REQUIRE(std::strcmp(f,
                      " 58 a sinol etybgnirts gs'tI ...esoppus  eb ot dt "
                      "regnolnats nahila drad") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_unaligned(f, g + 6, 9);
  REQUIRE(std::strcmp(f,
                      "b 58 a sgnol ety.gnirts  s'tI ..desoppusl eb ot ht "
                      "regnodnats nagila dra") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_unaligned(f, g + 7, 9);
  REQUIRE(std::strcmp(f,
                      "yb 58 a  gnol et..gnirtss s'tI . desoppuol eb otaht "
                      "regnadnats nngila dr") == 0);

  std::memset(f, 0, sizeof(f));
  copy_and_swap_64_unaligned(f, g + 8, 9);
  REQUIRE(std::strcmp(f,
                      "tyb 58 as gnol e...gnirtus s'tI t desoppnol eb onaht "
                      "regradnats mngila d") == 0);
}

TEST_CASE("copy_and_swap_16_in_32_aligned", "Copy and Swap") {
  // TODO(bwrsandman): test once properly understood.
  REQUIRE(true == true);
}

TEST_CASE("copy_and_swap_16_in_32_unaligned", "Copy and Swap") {
  // TODO(bwrsandman): test once properly understood.
  REQUIRE(true == true);
}

TEST_CASE("create_and_close_file_mapping", "Virtual Memory Mapping") {
  auto memory = xe::memory::CreateFileMappingHandle(
      L"test", 0x100, xe::memory::PageAccess::kReadWrite, false);
  REQUIRE(memory);
  xe::memory::CloseFileMappingHandle(memory, L"test");
}

}  // namespace test
}  // namespace base
}  // namespace xe
