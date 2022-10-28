# ldn_mitm

A mitm kip modified from fs_mitm.


ldn_mitm implements LAN connectivity by replacing the system's ldn service.

The original ldn service is only responsible for calling the WiFi service to scan and connect to nearby Switch. ldn_mitm uses the LAN UDP to emulate this scanning process. Therefore ldn_mitm is usually used with [`switch-lan-play`](https://github.com/spacemeowx2/switch-lan-play). A configuration tutorial can be found [here](http://www.lan-play.com/install).

## Version table

Please try the [GHA nightlies](https://github.com/spacemeowx2/ldn_mitm/actions) if you have updated beyond the supported AMS versions.
| ldn_mitm version | Atmosphère version |
| :--------------: | :----------------: |
| [1.14.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.14.0)            | [1.4.0](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/1.4.0)               |
| [1.13.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.13.0)            | [1.3.1](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/1.3.1)               |
| [1.12.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.12.0)            | [1.2.5](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/1.2.5)               |
| [1.11.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.11.0)            | [1.2.1](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/1.2.1)/[1.2.2](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/1.2.2)               |
| [1.10.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.10.0)            | [1.1.1](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/1.1.1)               |
| [1.9.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.9.0)            | [1.0.0](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/1.0.0)               |
| [1.8.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.8.0)            | [0.19](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.19.0)/[0.19.1](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.19.1)               |
| [1.7.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.7.0)            | [0.16.1](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.16.1)/[0.16.2](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.16.2)/[0.17.0](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.17.0)/[0.18.0](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.18.0)/[0.18.1](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.18.1) |
| [1.6.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.6.0)            | [0.15.0](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.15.0)/[0.14.4](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.14.4)   |
| [1.5.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.5.0)            | [0.14.0](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.14.0)/[0.14.1](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.14.1)        |
| [1.4.0](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.4.0)            | [0.13](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.13.0)               |
| [1.3.4](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.3.4)            | [0.11](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.11.0)/[0.12](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.12.0)          |
| [1.3.3](https://github.com/spacemeowx2/ldn_mitm/releases/tag/v1.3.3)            | [0.10](https://github.com/Atmosphere-NX/Atmosphere/releases/tag/0.10.0)               |

## Development

Make sure that the submodule is initialized.

```bash
git submodule update --init --recursive
```


### Using Docker

1. Install `Docker` and `docker-compose`.

2. Run `docker-compose up --build`. It runs `make -j8` in the container.


### Using devkitPro

1. Install [`devkitPro`](https://devkitpro.org/wiki/Getting_Started) and install `switch-dev`, `libnx` using `dkp-pacman`.

2. Run `make` command.

Licensing
=====

This software is licensed under the terms of the GPLv2, with exemptions for specific projects noted below.

You can find a copy of the license in the [LICENSE file](LICENSE).

Exemptions:
* The [yuzu Nintendo Switch emulator](https://github.com/yuzu-emu/yuzu) and the [Ryujinx Team and Contributors](https://github.com/orgs/Ryujinx) are exempt from GPLv2 licensing. They are permitted, each at their individual discretion, to instead license any source code authored for the ldn_mitm project as either GPLv2 or later or the [MIT license](https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/licensing_exemptions/MIT_LICENSE). In doing so, they may alter, supplement, or entirely remove the copyright notice for each file they choose to relicense. Neither the ldn_mitm project nor its individual contributors shall assert their moral rights against any of the aforementioned projects.
* [Nintendo](https://github.com/Nintendo) is exempt from GPLv2 licensing and may (at its option) instead license any source code authored for the ldn_mitm project under the Zero-Clause BSD license.

