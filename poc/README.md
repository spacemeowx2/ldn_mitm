# PoC: ldn_mitm 去掉 Atmosphere-libs（只依赖 libnx）

目标：把 `ldn_mitm` 对 `Atmosphere-libs`（libstratosphere + libvapours，683 个编译单元）
的依赖换成 **只依赖 libnx + `sm` 服务的 mitm 接口**。

`ldn_mitm/` 下的原有代码**尚未改动**；本目录是替代实现 + 验证环境。

---

## 现在完成到什么程度

| 部分 | 状态 |
| --- | --- |
| `sm` mitm 客户端（Install / Acknowledge / Uninstall） | ✅ libnx-only，`src/sm_mitm.c` |
| CMIF/hipc 服务端（含 domain、out object、copy/move handle、server pointer） | ✅ `src/ipc_server.{hpp,cpp}` |
| `ICommunicationService`（30 条命令） | ✅ `src/ldn_services.cpp` |
| `IClientProcessMonitor` / `ILdnConfig` | ✅ |
| `ShouldMitm` 查询会话 | ✅ `src/mitm_query_service.hpp` |
| 业务逻辑（LANDiscovery / LANProtocol / types） | ✅ 已去掉 stratosphere 依赖 |
| 日志（原 `ams::log` → `ams::fs`） | ✅ 改为可插拔 sink |
| host 端端到端测试 | ✅ **124 checks, 0 failures**（Linux + Windows） |
| aarch64 交叉编译（全部源文件） | ✅ `verify/cross_check.sh` |
| aarch64 **链接出真实 ELF** | ✅ `make` → `build/ldn_mitm_poc.elf`（text 209 KB） |
| 打包可部署的 `exefs.nsp` | ✅ `make nsp` → 145 KB（原实现 169 KB） |
| KIP 运行配置 / 真机 | ⬜ 未做 |

### 相对原实现的取舍

| 组件 | 原来 | 现在 |
| --- | --- | --- |
| `sf::Out<T>` / `OutArray` / `AutoSelect` | libstratosphere 模板 | `Request`/`Reply` 的显式 buffer API |
| `sf::LargeData` / `PrefersPointerTransferMode` | 影响模板选 buffer 模式 | 空基类，服务端按实际描述符判断 |
| `os::Mutex` / `os::ThreadType` | libstratosphere | `LdnMutex`（libnx `Mutex`）+ libnx `Thread` |
| `os::SystemEvent` | libstratosphere | libnx `Event`（autoclear, 跨进程） |
| `ams::log` | 写 sdmc 文件 | `ams::log::SetSink()`，sysmodule 里装文件 sink |
| `util::TSNPrintf` | libstratosphere | `vsnprintf` |

---

## 验证流程（不依赖任何硬件）

`libnx` 的客户端代码（`switch/sf/service.h`、`cmif.h`、`hipc.h`、`tipc.h`）是
header-only 的可移植 C。因此可以在 PC 上：

1. 用 **真实的 libnx 客户端代码** 构造请求（与被游戏调用时字节完全一致）
2. 交给 **我们自己的服务端实现** 处理
3. 用 **模拟内核**（`harness/mock_kernel.cpp`）代替 Horizon 的
   session / port / TLS / handle 语义
4. 用 **模拟 sm**（`harness/mock_sm.cpp`）提供 `sm:` 端口与真实的 TIPC 线格式

测的是线格式，不是「mock 对 mock」。

### 三条命令

```sh
# 1) host 端到端测试（Linux/macOS；Windows 用 build.ps1）
cd poc/verify && LIBNX=~/project/libnx/nx/include ./build.sh

# 2) 所有目标源码的 aarch64 交叉编译（需要 devkitA64）
./cross_check.sh

# 3) 链接真实的 sysmodule ELF，并打包可部署的 nsp
cd poc && make nsp
```

输出：

```
$ ./build.sh
[case] basic cmif dispatch (libnx client -> our server)
[case] error results propagate
[case] out object -> move handle
[case] domain session: convert + object ids
[case] domain session: out object -> object id
[case] mitm forward-to-session
[case] session close
[case] mitm: install -> accept -> acknowledge -> dispatch -> forward
[case] ldn services: config / monitor / communication commands

124 checks, 0 failures

$ ./cross_check.sh
  debug.cpp                    OK
  ipc_server.cpp               OK
  ipinfo.cpp                   OK
  lan_discovery.cpp            OK
  lan_protocol.cpp             OK
  ldn_services.cpp             OK
  ldn_types.cpp                OK
  sm_mitm.c                    OK
all target sources compile for aarch64

$ make
  LD   build/ldn_mitm_poc.elf
   text    data     bss     dec     hex filename
 209682   16208   38904  264794   40a5a build/ldn_mitm_poc.elf
```

### 已经验证的行为

| 用例 | 验证点 |
| --- | --- |
| basic cmif dispatch | `SFCI`/`SFCO` 头、`CmifOutHeader` 布局、out 数据对齐、控制命令 3（pointer buffer size） |
| error results | 服务端 Result 原样回到客户端 |
| out object | 普通 session 用 move handle，domain 内用 object id |
| domain session | `ConvertCurrentObjectToDomain`、object id 路由、domain 回复头 |
| mitm forward-to-session | 返回 `ResultShouldForwardToSession` 时按原始请求转发（含 pid 打标），对端回复原样带回 |
| mitm 全链路 | `InstallMitm` → sm 路由 → `AcceptSession` → `AcknowledgeSession`（客户端身份 + forward 会话）→ 派发 → 返回对象 → 转发 → `ShouldMitm` → `Uninstall` 回落 |
| ldn 全命令 | 初始化/状态/AP/Station/网络信息/安全参数/网络配置/advertise/scan/connect/disconnect/finalize，以及 config 与 process monitor |
| buffer 两种形态 | `Scan` 小请求走 server pointer、大请求走 HIPC buffer；`SetAdvertiseData` 走 pointer；`GetNetworkInfo` 走 map-alias buffer |
| copy handle | `AttachStateChangeEvent` 返回事件可读句柄 |

### host 侧对 libnx 的 3 处补丁（脚本自动做）

1. `switch/arm/tls.h` → host 版（`tpidrro_el0` 在 PC 上不存在）
2. `switch/types.h` 的 `BITL(n)` → `(uint64_t)1<<(n)`（host 是 LLP64）
3. 测试缓冲用 `mock::AllocLow()` 分配在 1 TiB 以下 —— **HIPC 的 server pointer 描述符
   只有 ~40 位地址空间**，x86_64 的栈/堆地址会被截断。真机上所有用户地址都在这范围内，
   所以这是 mock 的移植性问题，不是实现问题。

---

## 目录

```
poc/
  Makefile                # aarch64 sysmodule 构建（仅 libnx）
  src/
    sysmodule_main.cpp    # 入口：装 mitm、起 accept 线程、装文件日志
    ipc_server.hpp/.cpp   # libnx-only CMIF 服务端
    sm_mitm.h/.c          # libnx-only sm mitm 客户端
    mitm_query_service.hpp
    ldn_services.hpp/.cpp # 4 个服务对象（全部命令）
    lan_discovery_interface.hpp  # 服务层与 LAN 实现的接缝
    lan_discovery.*/lan_protocol.*/ldn_types.*/ipinfo.*/debug.*  # 移植后的业务逻辑
  verify/
    build.sh / build.ps1  # host 端到端测试
    cross_check.sh        # aarch64 全量交叉编译
    harness/
      mock_kernel.*       # 模拟内核：session/port/TLS/handle/mitm 路由/低地址分配
      mock_sm.*           # 模拟 sm：TIPC 线格式
      fake_lan_discovery.*# 脚本化的 LAN 替身（host 用）
      libnx_host_runtime.*# libnx 的 .c 部分（event 等）+ nifm 桩
      hostshim/           # newlib <sys/lock.h>、<arm_acle.h>、<poll.h>
    tests/main.cpp        # 用例
```

## 工具链

见 [dev-env.md](dev-env.md)：devkitPro 官方源可直连（Cloudflare 只按 UA 拦），
aarch64 用 devkitA64 **GCC 15.2.0**（16.x 会让 pinned 的 Atmosphere-libs 编不过）。

---

## 下一步

1. 打包 `exefs.nsp`（npdmtool + elf2nso）并补 KIP 运行配置（heap、`__applet_type`）
2. 真机验证：装到 `atmosphere/contents/4200000000000010/exefs.nsp`，跑一个 ldn 游戏
3. 若验证通过，再把 `ldn_mitm/source/` 整体切到这套实现，并删掉 `Atmosphere-libs` 子模块
