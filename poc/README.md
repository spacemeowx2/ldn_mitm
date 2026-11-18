# PoC: ldn_mitm 去掉 Atmosphere-libs（只依赖 libnx）

目标：把 `ldn_mitm` 对 `Atmosphere-libs`（libstratosphere + libvapours，683 个编译单元）
的依赖换成 **只依赖 libnx + `sm` 服务的 mitm 接口**。

本目录是这条路线的工作区，`ldn_mitm/` 下的原有代码**尚未改动**。

---

## 为什么不是「删掉依赖」，而是「自己实现 IPC 服务端」

调研结论：

| 原来用的 libstratosphere 组件 | 能否直接用 libnx |
| --- | --- |
| `sm` 客户端 + mitm 安装/acknowledge | ✅ **Atmosphere 里本来就是纯 libnx C**，已移植为 `src/sm_mitm.c` |
| `sf` / `cmif` / `hipc` **服务端**（接收请求、派发、构造回复、domain 对象） | ❌ **libnx 的 `switch/sf/*.h` 只有客户端**，服务端要自己写 |
| `os`（线程/互斥/事件） | ✅ libnx 有 `Thread`/`Mutex`/`Event` |
| `lmem`（exp heap） | ⚠️ 少量代码可移植，或换 newlib malloc |
| `fs`（SD 读写日志/配置） | ⚠️ sysmodule 里 fsdev 不可靠，需裸 `fs` IPC |
| `util`/`log` | ✅ `vsnprintf` |

所以本 PoC 的核心是两块：

- **`src/ipc_server.{hpp,cpp}`**：约 450 行、只依赖 libnx 的 CMIF 服务端
  （解析请求、派发、构造回复、domain 对象、mitm 转发）
- **`src/sm_mitm.c`**：约 120 行、libnx-only 的 sm mitm 客户端
  （`InstallMitm` / `AcknowledgeSession` / `UninstallMitm`）

---

## 验证流程（不依赖任何硬件）

`libnx` 的客户端代码（`switch/sf/service.h`、`cmif.h`、`hipc.h`、`tipc.h`）是
header-only 的可移植 C。因此可以在 PC 上：

1. 用 **真实的 libnx 客户端代码** 构造请求（与被游戏调用时字节完全一致）
2. 交给 **我们自己的服务端实现** 处理
3. 用 **模拟内核**（`harness/mock_kernel.cpp`）代替 Horizon 的
   session / port / TLS / handle 语义
4. 用 **模拟 sm**（`harness/mock_sm.cpp`）提供 `sm:` 端口与真实的 TIPC 线格式

这样测的是线格式，而不是「mock 对 mock」。

### 一条命令

```sh
# Linux / macOS
cd poc/verify && LIBNX=~/project/libnx/nx/include ./build.sh

# Windows
powershell -ExecutionPolicy Bypass -File poc\verify\build.ps1
```

输出：

```
[case] basic cmif dispatch (libnx client -> our server)
[case] error results propagate
[case] out object -> move handle
[case] domain session: convert + object ids
[case] domain session: out object -> object id
[case] mitm forward-to-session
[case] session close
[case] mitm: install -> accept -> acknowledge -> dispatch -> forward

51 checks, 0 failures
```

只编译/运行、不重新同步头文件：`SYNC=0` / `-NoSync`；打开内核日志：`LDN_TEST_VERBOSE=1` / `-Verbose`。

### 已经验证的行为

| 用例 | 验证点 |
| --- | --- |
| basic cmif dispatch | CMIF 头（`SFCI`/`SFCO`）、`CmifOutHeader` 布局、out 数据对齐、`query pointer buffer size` 控制命令 |
| error results | 服务端 Result 原样回到客户端（`0x1234`） |
| out object -> move handle | 普通 session 返回对象时通过 `svcCreateSession` + move handle，客户端能继续调用子对象 |
| domain session | `ConvertCurrentObjectToDomain`、后续请求带 object id 路由、domain 回复头 |
| domain out object | domain 内新建对象返回 object id（而不是句柄） |
| mitm forward-to-session | 返回 `ResultShouldForwardToSession` 时，把**原始请求字节**转发给真实服务（含 pid 打标），并把对端回复原样带回 |
| **mitm 全链路** | `InstallMitm`(65000) → sm 把连接路由到 mitm 端口 → `AcceptSession` → `AcknowledgeSession`(65003) 拿到客户端身份 + 真实服务会话 → 派发 → 返回对象 → 转发 → `ShouldMitm` 查询会话 → `UninstallMitm` 后回落到真实服务 |
| session close | `serviceClose` 正确拆掉服务端 session（无泄漏/重复释放） |

其中 mitm 全链路走的是**真实的 TIPC 线格式**（请求 `type = cmd + 16`，响应首 u32 是
Result），被调用的是 `src/sm_mitm.c`，不是桩。

### 相对 libnx 头文件的两处 host 补丁（脚本自动做）

1. `switch/arm/tls.h` → 换成 host 版（`tpidrro_el0` 在 PC 上不存在）
2. `switch/types.h` 的 `BITL(n)` → `(uint64_t)1<<(n)`（Windows 上 `long` 是 32 位，
   而 AArch64 上是 64 位；这只是修正 host 的类型宽度假设）

其余 libnx 头文件与上游完全一致（`sync` 时会打印上游 commit）。

---

## 目录

```
poc/
  src/
    ipc_server.hpp/.cpp   # libnx-only CMIF 服务端（可编到 aarch64）
    sm_mitm.h/.c          # libnx-only sm mitm 客户端（Install/Acknowledge/Uninstall）
    mitm_query_service.hpp# 回答 sm 的 ShouldMitm 查询
  verify/
    build.sh / build.ps1  # 同步头文件 + 编译 + 运行
    extract_toolchain.ps1 # （Windows）解压 MSYS2 mingw-w64 包
    harness/
      mock_kernel.h/.cpp  # 模拟内核：session/port/TLS/handle/mitm 路由
      mock_sm.h/.cpp      # 模拟 sm：TIPC 线格式，InstallMitm/Ack/Uninstall
      hostshim/           # newlib <sys/lock.h>、<arm_acle.h> 的 host 替身
      libnx_include/      # (生成) libnx 头文件镜像
    tests/main.cpp        # 用例：以 libnx 客户端身份发请求
```

## 工具链

见 [dev-env.md](dev-env.md)。要点：devkitPro 官方源可直连（Cloudflare 只按 UA 拦），
host 验证只需要任意一个 GCC（`g++ -std=gnu++17`），不需要 devkitPro。

---

## 下一步

1. ~~移植 `sm` 的 mitm 安装/确认~~ ✅ 已完成
2. 把现有业务接口（`ICommunicationService`、`IClientProcessMonitor`、`ILdnConfig`）
   从 `AMS_SF_*` 宏迁到 `ServerObject::Invoke` 的 switch 派发
3. 补 auto-select buffer / server pointer（`Scan`、`SetAdvertiseData`）与 copy handle
   （`AttachStateChangeEvent`）
4. 补齐 `fs`（SD 日志）与 `os` 原语的 libnx 替代
5. 写新的 `ldn_mitm/Makefile`（只依赖 libnx）+ CI 校验真实构建
