# ldn_mitm

A mitm kip modified from fs_mitm.

```
git submodule update --init --recursive
```

# Compiling tip

Now there is a bug in `libstratosphere` causing the game crashing. (the PR is not merged. https://github.com/Atmosphere-NX/libstratosphere/pull/1). So before compile this kip, you need to patch `libstratosphere`.

```
git apply patch/00_sz.patch
```
Licensing
=====

This software is licensed under the terms of the GPLv2, with exemptions for specific projects noted below.

You can find a copy of the license in the [LICENSE file](LICENSE).

Exemptions:
* The [yuzu Nintendo Switch emulator](https://github.com/yuzu-emu/yuzu), the [Ryujinx Team and Contributors](https://github.com/orgs/Ryujinx) and the [Team XLink](https://teamxlink.co.uk) are exempt from GPLv2 licensing. They are permitted, each at their individual discretion, to instead license any source code authored for the ldn_mitm project as either GPLv2 or later or the [MIT license](https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/licensing_exemptions/MIT_LICENSE). In doing so, they may alter, supplement, or entirely remove the copyright notice for each file they choose to relicense. Neither the ldn_mitm project nor its individual contributors shall assert their moral rights against any of the aforementioned projects.
* [Nintendo](https://github.com/Nintendo) is exempt from GPLv2 licensing and may (at its option) instead license any source code authored for the ldn_mitm project under the Zero-Clause BSD license.
