# PoC: ldn_mitm 去掉 Atmosphere-libs（只依赖 libnx�?
目标：把 `ldn_mitm` �?`Atmosphere-libs`（libstratosphere + libvapours�?83 个编译单元）
的依赖换�?**只依�?libnx + `sm` 服务�?mitm 接口**�?
本目录是这条路线的工作区，`ldn_mitm/` 下的原有代码**尚未改动**�?
---

## 为什么不是「删掉依赖」，而是「自己实�?IPC 服务端�?
调研结论（详见对话记录）�?
| 原来用的 libstratosphere 组件 | 能否直接�?libnx |
| --- | --- |
| `sm` 客户�?+ mitm 安装/acknowledge | �?**Atmosphere 里本来就是纯 libnx C**（`svcConnectToNamedPort` + `tipcDispatch`�?|
| `sf` / `cmif` / `hipc` **服务�?*（接收请求、派发、构造回复、domain 对象�?| �?**libnx �?`switch/sf/*.h` 只有客户�?*，服务端要自己写 |
| `os`（线�?互斥/事件�?| �?libnx �?`Thread`/`Mutex`/`Event` |
| `lmem`（exp heap�?| ⚠️ 少量代码可移植，或换 newlib malloc |
| `fs`（SD 读写日志/配置�?| ⚠️ sysmodule �?fsdev 不可靠，需�?`fs` IPC |
| `util`/`log` | �?`vsnprintf` |

所以本 PoC 的核心就�?**`src/ipc_server.{hpp,cpp}`**：一个约 400 行�?只依�?libnx �?CMIF 服务端�?
---

## 验证流程（不依赖任何硬件�?
`libnx` 的客户端代码（`switch/sf/service.h`、`cmif.h`、`hipc.h`）是 header-only
的可移植 C。因此可以在 PC 上：

1. �?**真实�?libnx 客户端代�?* 构造请求（与被游戏调用时字节完全一致）
2. 交给 **我们自己的服务端实现** 处理
3. �?**模拟内核**（`harness/mock_kernel.cpp`）代�?Horizon �?   session / port / TLS / handle 语义

这样测的是线格式，而不是「mock �?mock」�?
### 一条命�?
```powershell
powershell -ExecutionPolicy Bypass -File poc\verify\build.ps1
```

输出�?
```
[case] basic cmif dispatch (libnx client -> our server)
[case] error results propagate
[case] out object -> move handle
[case] domain session: convert + object ids
[case] domain session: out object -> object id
[case] mitm forward-to-session
[case] session close

28 checks, 0 failures
```

只编�?运行、不重新同步头文件：`-NoSync`；打开内核日志：`-Verbose`�?
### 已经验证的行�?
| 用例 | 验证�?|
| --- | --- |
| basic cmif dispatch | CMIF 头（`SFCI`/`SFCO`）、`CmifOutHeader` 布局、out 数据对齐、`query pointer buffer size` 控制命令 |
| error results | 服务�?Result 原样回到客户端（`0x1234`�?|
| out object -> move handle | 普�?session 返回对象时通过 `svcCreateSession` + move handle，客户端能继续调用子对象 |
| domain session | `ConvertCurrentObjectToDomain`、后续请求带 object id 路由、domain 回复�?|
| domain out object | domain 内新建对象返�?object id（而不是句柄） |
| mitm forward-to-session | 返回 `ResultShouldForwardToSession` 时，�?*原始请求字节**转发给真实服务（�?pid 打标），并把对端回复原样带回 |
| session close | `serviceClose` 正确拆掉服务�?session（无泄漏/重复释放�?|

### 相对 libnx 头文件的两处 host 补丁（脚本自动做�?
1. `switch/arm/tls.h` �?换成 host 版（`tpidrro_el0` �?PC 上不存在�?2. `switch/types.h` �?`BITL(n)` �?`(uint64_t)1<<(n)`（Windows �?`long` �?32 位，
   �?AArch64 上是 64 位；这只是修�?host 的类型宽度假设）

其余 libnx 头文件与上游完全一致（`sync` 时会打印上游 commit）�?
---

## 目录

```
poc/
  src/
    ipc_server.hpp/.cpp   # 被测代码：libnx-only CMIF 服务端（可编�?aarch64�?  verify/
    build.ps1             # 同步头文�?+ 编译 + 运行
    extract_toolchain.ps1 # 解压 MSYS2 mingw-w64 包到 toolchain\msys
    harness/
      mock_kernel.hpp/.cpp# 模拟内核：session/port/TLS/handle
      hostshim/           # newlib <sys/lock.h>�?arm_acle.h> �?host 替身
      libnx_include/      # (生成) libnx 头文件镜�?    tests/main.cpp        # 用例：以 libnx 客户端身份发请求
```

## 工具�?
devkitPro 的源在部分网络下�?Cloudflare 拦截�?03），所�?host 验证用的�?MSYS2 �?**MinGW-w64 GCC 16.2**（与 devkitPro 同为 GCC 系）�?
```powershell
# 1) 下载 .pkg.tar.zst �?toolchain\（见 build.ps1 头注释里的包清单�?# 2) 解压 + 生成链接器所需桩文�?powershell -ExecutionPolicy Bypass -File poc\verify\extract_toolchain.ps1
```

> `extract_toolchain.ps1` 还会�?`libpthread.a` / `default-manifest.o` 两个
> 空桩：GCC 16 �?spec 会引用它们，�?MSYS2 拆包后这两个文件不在我们下载的包里�?
**aarch64 的真实构建（.nsp）仍然需�?devkitPro �?CI**，本 PoC 尚未改动
`ldn_mitm/Makefile`�?
---

## 下一�?
1. 移植 `sm` �?mitm 安装/确认（`smAtmosphereMitmInstall` / `...AcknowledgeSession`），
   �?`IpcServer::OnAcceptMitmSession` 接上，并�?mock �?`sm:` 端口做同样的端到端测�?2. 把现有业务接口（`ICommunicationService`、`IClientProcessMonitor`、`ILdnConfig`�?   �?`AMS_SF_*` 宏迁�?`ServerObject::Invoke` �?switch 派发
3. �?auto-select buffer / server pointer（`Scan`、`SetAdvertiseData`）与 copy handle
   （`AttachStateChangeEvent`�?4. 补齐 `fs`（SD 日志）与 `os` 原语�?libnx 替代
5. 写新�?`ldn_mitm/Makefile`（只依赖 libnx�? CI 校验真实构建
