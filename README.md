<p align="center">
    <a href="https://github.com/xenia-project/xenia/tree/master/assets/icon">
        <img height="120px" src="https://raw.githubusercontent.com/xenia-project/xenia/master/assets/icon/128.png" />
    </a>
</p>

<h1 align="center">Xenia - Xbox 360 Emulator</h1>

Xenia is an experimental emulator for the Xbox 360. For more information, see the
[main Xenia wiki](https://github.com/xenia-project/xenia/wiki).

**Interested in supporting the core contributors?** Visit
[Xenia Project on Patreon](https://www.patreon.com/xenia_project).

Come chat with us about **emulator-related topics** on [Discord](https://discord.gg/Q9mxZf9).
For developer chat join `#dev` but stay on topic. Lurking is not only fine, but encouraged!
Please check the [FAQ](https://github.com/xenia-project/xenia/wiki/FAQ) page before asking questions.
We've got jobs/lives/etc, so don't expect instant answers.

Discussing illegal activities will get you banned.

## Status

Buildbot | Status
-------- | ------
[Windows](https://ci.appveyor.com/project/q4a/xenia/branch/linux-q4a) | [![Build status](https://ci.appveyor.com/api/projects/status/t9u35cuu4l0259e1/branch/linux-q4a?svg=true)](https://ci.appveyor.com/project/q4a/xenia/branch/linux-q4a)
[Linux](https://travis-ci.org/q4a/xenia) | [![Build status](https://travis-ci.org/q4a/xenia.svg?branch=linux-q4a)](https://travis-ci.org/q4a/xenia)

Quite a few real games run. Quite a few don't.
See the [Game compatibility list](https://github.com/xenia-project/game-compatibility/issues)
for currently tracked games, and feel free to contribute your own updates,
screenshots, and information there following the [existing conventions](https://github.com/xenia-project/game-compatibility/blob/master/README.md).

## Disclaimer

The goal of this project is to experiment, research, and educate on the topic
of emulation of modern devices and operating systems. **It is not for enabling
illegal activity**. All information is obtained via reverse engineering of
legally purchased devices and games and information made public on the internet
(you'd be surprised what's indexed on Google...).

## Quickstart

See the [Quickstart](https://github.com/xenia-project/xenia/wiki/Quickstart) page.

## Building

See [building.md](docs/building.md) for setup and information about the
`xb` script. When writing code, check the [style guide](docs/style_guide.md)
and be sure to run clang-format!

## Contributors Wanted!

Have some spare time, know advanced C++, and want to write an emulator?
Contribute! There's a ton of work that needs to be done, a lot of which
is wide open greenfield fun.

**For general rules and guidelines please see [CONTRIBUTING.md](.github/CONTRIBUTING.md).**

Fixes and optimizations are always welcome (please!), but in addition to
that there are some major work areas still untouched:

* Help work through [missing functionality/bugs in games](https://github.com/xenia-project/xenia/labels/compat)
* Add input drivers for [DualShock4 (PS4) controllers](https://github.com/xenia-project/xenia/issues/60) (or anything else)
* Skilled with Linux? A strong contributor is needed to [help with porting](https://github.com/xenia-project/xenia/issues/1430)

See more projects [good for contributors](https://github.com/xenia-project/xenia/labels/good%20first%20issue). It's a good idea to ask on Discord and check the issues page before beginning work on
something.

## FAQ

See the [frequently asked questions](https://github.com/xenia-project/xenia/wiki/FAQ) page.
