# catnip_emulator 外设扩展实施方案：网口 + SD 卡

> 本文是 catnip_emulator 新增外设的**工作底稿**：核查结论、设计规格、逐步执行清单、验收方式。
> 每完成一步就更新 §11「进度」。方案里有分歧的地方记在 §10，先讨论再动手。
> 给人看的概览与进度看板在 [plan_overview.md](plan_overview.md)（不含寄存器级细节，只指到本文的章节号）。

---

## 0. 工作方式约定

- **逐步执行**：每次动手前先说明「要改哪些文件、改成什么、怎么验证」，等确认后再改。一次只做 §7 里的一个编号步骤。
- **每步都留证据**：给出可复现的命令与真实输出（`make` 的报错、`tb/eth_test` 的断言、u-boot 的 `ping` 输出）。不做没验证过的"应该没问题"结论。
- **每步讲清「为什么」**：动手前除了"改哪些文件、怎么验证"，再用**一两句**点透"为什么这么做"——这一步解决什么问题、为什么选这个做法（尤其要点出驱动/硬件依据里的坑）。默认**简要**即可，不复述代码在做什么；用户需要更深的推导或背景时会**追问**，那时再展开。
- **不擅自扩大范围**：不改内核/u-boot 的 DTS 与 defconfig，不动本文档没列的文件。
- **每步可回滚**：改动尽量局限在少数文件，随时能 `git checkout` 回到上一步。
- 有不确定的地方先问，不猜。

---

## 1. 目标与范围

### 本阶段做（网络）
`axi_intc_0`（0x10200000）+ `axi_ethernetlite_0`（0x10E00000）+ MDIO/PHY 桩 + 可插拔的宿主网络后端。
验收终点：Linux 下 guest 能 `ping` 通宿主，并能 `ssh`/`telnet` 到宿主（后端能力见 §6）。

### 后续做（SD，路线 a —— 已定）
按真机建模 **LogiSDHC（Arasan SDHCI 兼容，0x10300000）** + SD 卡协议模型 + `sdcard.img` 落盘。
验收终点：内核识别出块设备 `/dev/mmcblk0` 及其分区 `/dev/mmcblk0p1`，能 `dd` / `mount`。详见 §5.4、§5.5、§7 阶段 4。

### 明确不做
GPIO(0x10600000)；内核/u-boot 的 DTS 与 defconfig 改动（**限 M0–M5 期间**；唯一例外见 §10「已登记」的目标 A）；SD-over-SPI 路线（已勘定，见 §10 备查）。

---

## 2. 事实基础（核查记录）

### 2.1 模拟器现状（catnip_emulator/）

| 位置 | 内容 |
|---|---|
| `src/bus/axi.h` | `AXI32_Slave(base, len)`：子类只实现 `readw/writew`（`readb/writeb` 有默认实现），1/2/4 字节与 byte-mask 拆解在基类；`AXI32_Interconnect(base,len)` + `addSlave()`；`AXI32_RAM(base,len,width)` + `loadBin/loadBytes` |
| `src/dev/uart16550.{h,cpp}` | 唯一的外设范例：构造函数带 `regOffset`（对应 DT `reg-offset`）、`setIRQLine(bool*)`、`tick()`、`openConsole()`；类内**不含绝对地址** |
| `src/soc/catnipsoc.{h,cpp}` | `bus`/`cpu`/`memory`(128MB)/`bootrom`/`uart` 五个成员；`regionName(addr)` 返回 `"memory"/"uart"/"bootrom"/"unmapped"`；中断接线范例：`uart.setIRQLine(cpu.get_irq(CATNIPSOC_UART_IRQ))` |
| `src/cpu/mips32_core.cpp` | `get_irq(n)` 返回 `bool*`（n 为 MIPS IP 号，>7 返回 NULL）——level 型中断线，已有中断通路（UART→IP2）被真实内核验证可用 |
| `src/main.cpp` | 参数：`<image>[@<phys>]` 与 `--entry <pc>`；**至少要给一个 image**；没有设备类选项 |
| `Makefile` | `VPATH = src src/common src/bus src/cpu src/dev src/soc tb`；`CEMU_OBJ`/`NSC_OBJ`/`UART_OBJ` 三组目标文件；`all:` 产出 `cemu`、`tb/nscscc_tests`、`tb/uart_test` |
| `tb/uart_test.cpp` | 设备自测范例：只链接 `uart_test.o + uart16550.o + axi.o`，**不依赖 CPU/SoC** |
| 其它 | 仓库根有 `uart_notes.txt`（笔记文件先例）、`build/`、`cemu`、`.vscode/` |

**由此得到的设计约束**：新设备必须能脱离 CPU/SoC 单独编译自测（像 uart_test 那样），所以设备只通过 `bool*` 中断线对外通信，不反向依赖 CPU。

### 2.2 真机 CatnipSoC（Vivado 工程 `/media/sf_win_workspace/Catnip_Project/CatnipSoC/`）

MangoMIPS32 `m_axi` 上的从设备（`CatnipSoC.srcs/sources_1/bd/catnipsoc_top/catnipsoc_top.bd` 的 addressing）：

| 基址 | 范围 | 实例 / IP | 模拟器现状 |
|---|---|---|---|
| 0x10200000 | 64K | `axi_intc_0`：`xlnx,xps-intc-1.00.a`，3 输入，`xlnx,kind-of-intr = <0x1>` | 未建模 |
| 0x10300000 | 64K | `logisdhc_0`：logicbricks `logisdhc` v2.2（**Arasan SDHCI 兼容的 SD 主机**，`C_SD_BASE_CLOCK_FREQ=100`） | 未建模 |
| 0x10400000 | 64K | `axi_uart16550_0` | 已建模 |
| 0x10600000 | 64K | `axi_gpio_0` | 未建模（不在范围） |
| 0x10E00000 | 64K | `axi_ethernetlite_0`（xps-ethernetlite）+ `mii_to_rmii_0` | 未建模 |
| 0x1D000000 | 16M | `spi_flash_controller`：AXI Quad SPI 3.2，**AXI4 memory-mapped（XIP）**，`C_SPI_MODE=0`、`C_NUM_SS_BITS=1`、`C_SPI_MEM_ADDR_BITS=24`、`C_FIFO_DEPTH=16` | 不在范围 |
| 0x1FC00000 | 1M | `axi_bram_ctrl_0`（bootrom） | 已建模 |

中断拓扑（bd nets，与 DTS 一致）：
```
axiethernetlite.ip2intc_irpt ─┐
~eth_intn (PHY 中断, 顶层引脚) ─┼→ intc_irq_concat → axi_intc_0.intr ─┐
logisdhc.sd_int ──────────────┘                                     │
axi_uart16550.ip2intr ─────────────→ intr_concat_0.intr0 ──────────┼→ CPU intr
axi_intc_0.irq ────────────────────→ intr_concat_0.intr1 ──────────┘
```
即：UART→CPU IP2（绕过 INTC），MAC/PHY/SD→INTC 输入 1/0/2，INTC→CPU IP3。

HDL 位置：`.../ipshared/592c/hdl/src/vhdl/{logisdhc,sdhc_card_if,sdhc_host_if,standard_dma_ctrl}.vhd`（LogiSDHC 说明书：logicbricks.com，v2.2）。
AXI Quad SPI 的 `C_TYPE_OF_AXI4_INTERFACE=1`，且它的 AXI4-Lite 控制口**没有**接进 CPU 的地址映射 → 真机上它不是可用的 SD 通路。

### 2.3 软件可用性（好消息：网络与 SD 都不需要改软件）

| 项 | 内核 | u-boot |
|---|---|---|
| 以太网节点 | `arch/mips/boot/dts/catnipsoc/catnipsoc.dts`：`ethernet@10e00000`，`compatible = "xlnx,xps-ethernetlite-3.00.a"`，`interrupts = <1>`（走 axi_intc_0），`phy-handle = <&phy0>`，`local-mac-address = [08 86 4C 0D F7 09]`，`xlnx,rx-ping-pong/tx-ping-pong = <0x1>` | `arch/mips/dts/catnipsoc.dts`：同 IP 但 `compatible = "xlnx,xps-ethernetlite-1.00.a"`，**无 interrupt-parent / 无 intc 节点** |
| 以太网驱动 | `CONFIG_NET_VENDOR_XILINX=y`、`CONFIG_XILINX_EMACLITE=y`；`CONFIG_PHYLIB/SWPHY/SMSC_PHY/FIXED_PHY=y` | `CONFIG_XILINX_EMACLITE=y`、`CONFIG_DM_ETH=y`、`CONFIG_MII=y`、`CONFIG_PHYLIB=y`、`CONFIG_CMD_PING=y`、`CONFIG_CMD_TFTPBOOT=y`；`CONFIG_PHY_SMSC is not set`、`CONFIG_CMD_MII is not set`（走 generic PHY） |
| SD 节点 | `mmc@10300000`：`compatible = "arasan,sdhci-8.9a"`，`interrupts = <2>`（走 axi_intc_0），`clocks = <&ext>,<&ext>`（50MHz），无 `vmmc-supply`/`cd-gpios`/`bus-width` | **完全没有 SD**（`# CONFIG_MMC is not set`） |
| SD 驱动 | `catnipsoc_sd_defconfig`：`CONFIG_MMC=y`、`MMC_SDHCI`、`MMC_SDHCI_PLTFM`、`MMC_SDHCI_OF_ARASAN`、`MMC_BLOCK=y`（默认 `.config`/`catnipsoc_defconfig` 里 MMC 是关的） | 无关 |
| SPI | `# CONFIG_SPI is not set`（三份 config 都是），DTS 无 spi 节点 | `# CONFIG_SPI is not set` |

**结论**：网络两侧都开箱即用；SD 只需要换成 `catnipsoc_sd_defconfig` 重编内核，DTS 与驱动都已就位。

### 2.4 关键驱动语义（写模型前必须遵守的点）

**AXI INTC**（`linux-5.6.14-catnipsoc/drivers/irqchip/irq-xilinx-intc.c:23-33`，只用这 8 个寄存器）：
```c
#define ISR 0x00   IPR 0x04   IER 0x08   IAR 0x0c
#define SIE 0x10   CIE 0x14   IVR 0x18   MER 0x1c
#define MER_ME (1<<0)   MER_HIE (1<<1)
```
- probe 序列：`IER←0`，`IAR←0xFFFFFFFF`（写 1 清），`MER←MER_HIE|MER_ME`，**再读回 MER 校验**：读不回 0x3 就误判为 big-endian 并改走 BE 访问。
- ack 是 `write(IAR, 1<<hwirq)`（**只写不读**）；level 型在 unmask 时先写 IAR 再写 SIE。
- `IVR`：无中断必须返回 `0xFFFFFFFF`，否则 `do{...}while(true)` 死循环。
- `xlnx,kind-of-intr = <0x1>`：bit=1 → 输入是**边沿**型 → 输入 0（PHY）是边沿，输入 1/2（MAC/SD）是电平。

**xps-ethernetlite**（`drivers/net/ethernet/xilinx/xilinx_emaclite.c:31-85`，u-boot 侧同布局）：
```
0x0000 TX ping 缓冲   0x07E4 MDIOADDR   0x07F4 TPLR(ping)   0x07FC TSR(ping)
0x0800 TX pong 缓冲   0x07E8 MDIOWR     0x0FF4 TPLR(pong)   0x0FFC TSR(pong)
0x1000 RX ping 缓冲   0x07EC MDIORD     0x100C RPLR(ping)   0x17FC RSR(ping)
0x1800 RX pong 缓冲   0x07F0 MDIOCTRL   0x180C RPLR(pong)   0x1FFC RSR(pong)
                      0x07F8 GIER
```
| 行为 | 模型的正确做法 | 写错的后果 |
|---|---|---|
| 写 TSR 置 `BUSY\|ACTIVE`(0x80000001) | 按 TPLR 长度从 ping/pong 缓冲取帧发出去；完成后**清 BUSY、保留 ACTIVE** | 驱动用 `(TSR & (BUSY\|ACTIVE))==0` 判缓冲可用；两个位都清则 `xemaclite_interrupt` 永远看不到 `BUSY==0 && ACTIVE!=0` → `netif_wake_queue` 不调用 → tx 队列卡死 |
| 写 TSR 置 `BUSY\|PROGRAM`(0x3) | 从缓冲取 6 字节 MAC 记下，**清掉 bit0/bit1**，不发送 | `xemaclite_update_address` 的 `while (TSR & 0x3);` 死循环（u-boot 与内核都会走这里）|
| 收到帧 | 写进当前 RX 缓冲、写 RPLR 长度、置 `RSR = RECV_IE(0x8)\|RECV_DONE(0x1)`、按需拉中断 | 内核靠 `RSR & 0x1` 判有帧；**帧长是靠帧内 EtherType 推的**，所以必须写完整以太帧（含 14 字节头） |
| 写 RSR 清 `RECV_DONE` | 该缓冲置空，可装下一帧 | 缓冲被占死，只收得到一帧 |
| 中断输出 | `GIER.GIE(0x80000000) && ((RSR.RECV_DONE && RSR.RECV_IE) \|\| (TSR 可发送 && TSR.XMIT_IE))` | Linux 完全收不到包（它没有 NAPI/轮询路径，`xemaclite_interrupt` 是唯一收取路径） |
| MDIO 事务 | 写 `MDIOCTRL.bit0(MDIOSTS)` 启动，**同步完成并自清该位**；`MDIOADDR.bit10=1` 为读，结果放 `MDIORD`；`MDIOCTRL.bit3(MDIOEN)` 必须能读回 | u-boot `mdio_wait` 等 MDIOSTS 变 0（超时 2000ms），不自清直接探测失败 |
| PHY 探测 | `BMSR` 满足 u-boot 掩码 `(v & 0x1808) == 0x1808`；PHYID 非 0xFFFF | u-boot 会判定"无 PHY"，内核 genphy/smsc 也绑定不上 |

**u-boot 与内核的差异**：u-boot 只认 `1.00.a`、**纯轮询**（读 TSR/RSR，不用中断）；内核认到 `3.00.a`、**RX 完全依赖中断**。→ 先用 u-boot 验证寄存器语义，再引入 INTC。

**SDHCI（arasan 8.9a）**：`drivers/mmc/host/sdhci-of-arasan.c:351`（quirks）、`:543`（`"arasan,sdhci-8.9a"`）。用到的 quirks：
`SDHCI_QUIRK_CAP_CLOCK_BASE_BROKEN`（忽略 CAPS 里的 base clock，用 `clk_xin` = DT 的 50MHz `ext`）、`SDHCI_QUIRK_BROKEN_PRESET_VALUE`（不碰 preset 寄存器）、`CLOCK_DIV_ZERO_BROKEN`、`STOP_WITH_TC`；DT 无 `arasan,soc-ctl-syscon` → **不需要 syscon**。

---

## 3. 设计

### 3.1 文件与类

| 文件 | 类 | 职责 |
|---|---|---|
| `src/dev/xps_intc.{h,cpp}`（新） | `XpsIntc` | 8 个寄存器、3 输入、`setInputLine(n,bool*)`、输出一根 `bool*`；不做地址硬编码 |
| `src/dev/ethernetlite.{h,cpp}`（新） | `EthernetLite` | MAC 寄存器 + ping/pong 缓冲 + 中断条件 + `tick()` 轮询后端 |
| `src/dev/mdio_phy.{h,cpp}`（新） | `MdioPhyStub` | SMSC 兼容 PHY 寄存器桩（被 `EthernetLite` 持有）|
| `src/net/backend.h`（新） | `NetBackend` | 抽象：`sendFrame/recvFrame/tick` |
| `src/net/backend_null.cpp` `backend_slirp.cpp`（新） | — | **首期两种**后端（`null` 自测 + `slirp`）；`backend_socket.cpp`/`backend_tap.cpp` 留作可选，不在首期 |
| `src/dev/sdhci.{h,cpp}`（新，阶段 4） | `Sdhci` | SDHCI 主机寄存器 + 命令触发 + DMA |
| `src/dev/sdcard.{h,cpp}`（新，阶段 4） | `SdCard` | SD 卡协议 + CID/CSD/SCR + `sdcard.img` 读写 |
| `src/soc/catnipsoc.{h,cpp}` | — | 新增常量、成员、bus 挂载、中断接线、`regionName` 名字 |
| `src/main.cpp` | — | 新增 `--net`、`--hostfwd`、`--mac`（阶段 4 再加 `--sdcard`）|
| `Makefile` | — | 新 `*.o` 进 `CEMU_OBJ`；新 `tb/eth_test`（+ 阶段 4 的 `tb/sd_test`）目标；VPATH 若新增 `src/net` 要加一项 |
| `tb/eth_test.cpp`（新） | — | 寄存器级自测，只链接 `eth_test.o + ethernetlite.o + axi.o`（+ 必要后端桩） |

### 3.2 命名与约定（遵守仓库既有风格）

- 文件名/目录全小写、无下划线以外的修饰；设备类内**不写死绝对地址**，`base/len` 由例化处传入（照 `UART16550(base,len,regOffset)`）。
- 寄存器偏移用 `#define ETH_xxx 0x...` 形式（照 `UART16550_RBR` 的写法），注释里标明来源（驱动文件:行）。
- 设备只暴露 `bool*` 中断线；**不依赖 CPU/SoC**，保证能单独跑自测。
- 注释只在"为什么"层面写（寄存器的坑、驱动依赖），不叙述代码行为。

### 3.3 不变量

- 不改 `src/bus/axi.{h,cpp}`（现有抽象够用）。
- 不改内核/u-boot 的 DTS、config（**限 M0–M5 期间**；唯一例外见 §10「已登记」的目标 A）。
- 不改现有测试的行为；`make` + `make run` 必须在每一步之后仍然可用。

---

## 4. 中断接线（SoC 侧）

```
EthernetLite.irq ──→ XpsIntc.input[1]
MdioPhyStub.irq ──→ XpsIntc.input[0]     (边沿型，见风险 §9)
SdCard/Sdhci.irq ─→ XpsIntc.input[2]     (阶段 4)
XpsIntc.irq ──────→ MIPS32_Core::get_irq(3)
UART16550 ────────→ MIPS32_Core::get_irq(2)   (现状，不动)
```

---

## 5. 设备规格

（`M` = 需实现的寄存器/行为；`W` = 写模型时容易错的地方）

### 5.1 XpsIntc

- `M` ISR/IPR/IER/IAR/SIE/CIE/IVR/MER 八个寄存器；`IAR` 写 1 清；`SIE/CIE` 置/清 IER；`IVR` 返回最高优先级挂起号，无中断返回 `0xFFFFFFFF`；`MER` 可读回。
- `W` 边沿输入（input 0）与电平输入（input 1/2）分开处理：电平输入"输入仍高则 pending 跟随"，但被 IAR 清掉后不能自己无限重挂（驱动在 unmask 时才补 ack）。
- `W` probe 读回 MER 的校验（决定字节序）。

### 5.2 EthernetLite

- `M` 见 §2.4 的表：TPLR/TSR 收发、RPLR/RSR、MAC 编程序列、GIER 中断条件、ping/pong 双缓冲。
- `M` `tick()`：每 N 个 cycle 调一次（照 `UART16550::pollCount` 的写法），从后端取帧、非阻塞。
- `M` 收到帧时写入的缓冲按 RSR 轮转（ping→pong），与驱动 `next_rx_buf_to_use` 的期望一致。
- 决定：**不建模 FCS**（内核按 `+ETH_FCS_LEN` 多读 4 字节，落在缓冲残余区，不影响 IP 栈；TX 侧驱动也只写真实帧长）。
- 决定：**不做 MAC 地址过滤**（先全收）。若日后要多播/广播行为更真，再按 DT `local-mac-address` 加过滤。

### 5.3 MdioPhyStub

- `M` MDIO 4 寄存器握手（`MDIOSTS` 同步自清、`MDIOEN` 可读回）。
- `M` 寄存器桩：`PHYID1=0x0007`、`PHYID2=0xC0F1`（LAN8710/8720 家族，让内核 `smsc.c` 与 u-boot 都能识别）；`BMSR` 置 `LINK_STATUS|ANEG_COMPLETE` 且满足 `(v&0x1808)==0x1808`，锁存位读清；`BMCR`/`ANAR`/`ANLPAR` 报 10/100 全双工 + 自协商。
- `W` 链路状态与 PHY 中断的配合（见 §9 风险）。

### 5.4 Sdhci（阶段 4）

- `M` 标准 SDHCI 寄存器集（0x00–0xFF）：参数/命令/传输模式/块大小/块数、响应寄存器组、`PRESENT_STATE`（`CARD_INSERTED`）、中断 status/enable/signal-enable、错误状态、时钟控制（**须能回读驱动写入的分频**）、power、timeout ctrl、software reset（含两个子位）。
- `M` 命令触发：写 `CMD` → 生成响应 → 置 status → （可选中断）。
- `M` 数据通路：SDMA / ADMA2 描述符 → 直接读写 `AXI32_RAM`（同步完成）。
- quirks 带来的简化：不实现 preset 寄存器；`CAPS` 的 base clock 无关紧要；`max_clk` = 50MHz。
- 参考实现：QEMU `hw/sd/sdhci.c`（取子集）。

### 5.5 SdCard + sdcard.img（阶段 4）

- `M` 命令集：CMD0/8/ACMD41/CMD2/CMD3/CMD9/CMD7/CMD13/CMD16/CMD17/CMD18/CMD23/CMD24/CMD25/CMD12（+ ACMD6 设 4bit）；响应 R1/R1b/R2/R3/R6/R7。
- `M` 寄存器镜像：CID/CSD/SCR/SSR/OCR（`SDHC` 容量模式，块寻址）。
- `M` 镜像落盘：打开 `sdcard.img`，读/写扇区，写回（可选 `--sd-readonly`）。
- 简化决定：**不校验也不生成 CRC**（模型同时扮演主机控制器，没有别的东西校验它）；**总线宽度只当字节流**（1/4bit 对模型无差别）；高容量/高速（CMD6）可后置。
- 参考实现：QEMU `hw/sd/sd.c`、`hw/sd/sdmmc-internal.c`。

---

## 6. 宿主网络后端

`NetBackend` 接口（设备侧只认这个缝，换后端不动设备）：

```cpp
class NetBackend {
public:
    virtual ~NetBackend() {}
    virtual void sendFrame(const u8 *frame, u32 len) = 0;    // 客机 → 宿主
    virtual bool recvFrame(u8 *buf, u32 cap, u32 &len) = 0;  // 宿主 → 客机（非阻塞）
    virtual void tick() {}                                   // slirp 要喂定时器
};
```

| 后端 | 给到的效果 | 前置条件 |
|---|---|---|
| `null` | 回环（发出即收回），供自测 | 无 |
| `socket` | UDP 点对点：不是真网卡，但能立刻验证整条链路 | 无 |
| `slirp` | 客机 10.0.2.15/24，宿主=10.0.2.2（= 宿主的 loopback），DNS=10.0.2.3；客机可 `ping`/`ssh`/`telnet` 宿主，TCP/UDP 可出公网（**`ping` 公网不通**）；自带 TFTP 服务（`tftp_path`）；宿主用 `--hostfwd tcp:127.0.0.1:2222-:22` 反连客机 | 本机**未安装** libslirp（无 `libslirp.pc`/头文件），需 `apt install libslirp-dev` |
| `tap` | 客机成为宿主局域网上的真实 L2 主机，双向可连 | 建 tap+bridge 需 root/CAP_NET_ADMIN（本机 `/dev/net/tun` 已是 `crw-rw-rw-`，但 `TUNSETIFF` 仍需权限）；宿主是 VirtualBox 虚拟机（enp0s3/enp0s8），要桥出去得先起 br0 |

**已定（2026-09-21）：M2 的真实后端用 `slirp`。** 理由：

- M2 的验收包含 **`tftpboot` 取文件**，而 **libslirp 自带 TFTP 服务**（已核实：上游 `src/libslirp.h` 的 `SlirpConfig` 里就有 `tftp_path` / `tftp_server_name` / `bootfile` 字段；QEMU 文档原话 "When using the built-in TFTP server, the router is also the TFTP server."）——只需把 `tftp_path` 指向宿主的一个目录，**宿主不用装 tftpd**。
- 走 `socket` 的话，对端要在**帧层**实现 ARP + IPv4 + UDP + 完整的 TFTP 状态机（几百行），是三个选项里成本最高的（`ping` 的对端只要几十行，`tftpboot` 的对端不是同一个量级）。
- 走 `tap` 需要 root + 改宿主网络；本机宿主又是 VirtualBox 虚拟机（`enp0s3` = VBox NAT `192.168.3.56`，`enp0s8` = host-only 无 IP），桥接不可靠，代价最高。

首期只实现 `null` + `slirp` 两个后端；`socket` / `tap` 保留为将来的可选后端（本节的后端接口缝不变，换后端不动设备代码）。

**slirp 的部署口径与边界**（写模型、写验收时按这个来）：客机 `10.0.2.15/24`、宿主 `10.0.2.2`（= 宿主的 loopback）、DNS `10.0.2.3`；`ping 10.0.2.2` 通，**`ping` 公网不通**（ICMP echo 出网需要特权），TCP/UDP 出网正常。

**前置（实测）**：`libslirp-dev` 本机未安装（无 `.pc` / 头文件）；宿主也没有 tftpd、没有 sshd 在监听 22。**动宿主（装包）之前先经用户确认。**

---

## 7. 微步骤清单（逐步执行，一次一步）

> 每步的「验收」都要真的执行并贴出结果。步骤顺序按"先易后难、先轮询后中断"排。

### 阶段 0：脚手架（不改行为）

- **0.1** 新建 `src/dev/ethernetlite.{h,cpp}` 骨架：`EthernetLite : AXI32_Slave`，寄存器偏移宏齐备、`readw/writew` 先只记账（返回 0/丢弃），加进 `Makefile` 的 `CEMU_OBJ`。
  - 触碰：`src/dev/ethernetlite.{h,cpp}`、`Makefile`
  - 验收：`make` 通过；`make run` 与之前完全一致（u-boot/内核照常启动）
- **0.2** 在 `src/soc/catnipsoc.{h,cpp}` 加地址常量（`CATNIPSOC_ETH_BASE/LEN` 等）与成员，挂到 `bus`，`regionName()` 认新区域。
  - 触碰：`src/soc/catnipsoc.{h,cpp}`
  - 验收：`make` 通过；`make run` 正常；`regionName` 对 0x10E00000 返回新名字

### 阶段 1：EthernetLite 行为 + MDIO/PHY + 自测

- **1.1** TX 路径：TPLR 取长度、ping/pong 选择、`BUSY|ACTIVE` 触发、完成后"清 BUSY 留 ACTIVE"；先接一个内置回环（`null` 后端不落地文件，先写 `tb/eth_test.cpp` 的桩）。
- **1.2** MAC 编程序列（`BUSY|PROGRAM` → 取 6 字节、清 bit0/bit1）。
- **1.3** RX 路径：写缓冲 + RPLR + `RSR.RECV_DONE`，清位后可复用。
- **1.4** 中断条件（GIER/IER 位）+ `setIRQLine(bool*)`。
- **1.5** MDIO 4 寄存器 + `MdioPhyStub`（PHYID/BMSR/BMCR/ANAR/ANLPAR）。
- **1.6** `tb/eth_test.cpp` 补全 + `Makefile` 新增 `tb/eth_test` + `tb/Makefile` 新增 `eth-test` 入口。
  - 阶段 1 的验收（每步都做，1.6 汇总）：`make && make -C tb eth-test` 全绿；断言覆盖 §2.4 表里每个 `W`。

### 阶段 2：后端 + u-boot 验收（不需要 INTC）

- **2.1** `src/net/backend.h` + `src/net/backend_null.cpp`（回环）+ `Makefile` VPATH 加 `src/net`。
  - 目录待 §10.2 定；若并入 `src/dev/`，则不加 VPATH。
- **2.2** 接 `slirp` 后端：`src/net/backend_slirp.cpp`（`SlirpConfig` 里配 `vnetwork`/`vhost`/`vdhcp_start`、`tftp_path`、`bootfile`），链接 libslirp（`pkg-config --cflags --libs libslirp`）。
  - **开工前需用户确认安装 `libslirp-dev`**（当前未装）。
  - 首期只做 `null` + `slirp`；`socket`/`tap` 作为将来可选后端，缝不变。
- **2.3** `main.cpp` 加 `--net none|null|slirp`（+ `--tftp-path` 之类选项与 `usage()` 文案），运行时创建后端并交给 `EthernetLite`。
- **2.4** **u-boot 验收**（后端 = slirp）：`make run` → 提示符下依次敲 `dhcp`（客机拿 `10.0.2.15`，`serverip` 自动 = `10.0.2.2`）→ `ping 10.0.2.2`（通）→ `tftpboot 0x81000000 uImage.bin`（从宿主目录取到 uImage）。
  - u-boot 侧 `CONFIG_CMD_TFTPBOOT` / `CONFIG_CMD_BOOTP`(+DHCP/GATEWAY/SUBNETMASK) / `CONFIG_CMD_PING` 均已就绪；TFTP 服务由 libslirp 内置（`tftp_path`），宿主**不需要**装 tftpd。
  - 这一步是整件事的去风险点：**纯轮询驱动**证明 MAC 寄存器语义正确，此时完全没有 INTC。

### 阶段 3：AXI INTC + Linux 验收

- **3.1** `src/dev/xps_intc.{h,cpp}`（8 寄存器、边沿/电平、IVR 0xFFFFFFFF）+ `tb/intc_test.cpp` + Makefile 目标。
- **3.2** SoC 接线：`eth.irq→intc.input[1]`、`phy.irq→intc.input[0]`、`intc.irq→cpu.get_irq(3)`；`regionName` 加 0x10200000。
- **3.3** **Linux 验收**：`make run_linux` → `ifconfig eth0 up` 显示 `Link is up`；`ping 10.0.2.2` 通；`telnet`/`ssh` 到宿主；`ip -s link` 看到 tx/rx 计数增长。
- **3.4**（可选）`--hostfwd` 反连、`--mac`、`make help`/README 补文案。

### 阶段 4：SD 路线 a（网络收尾后开始）

- **4.1** `src/dev/sdhci.{h,cpp}` 骨架 + `tb/sd_test.cpp`：寄存器可读写、`CAPS`/`PRESENT_STATE`、software reset、时钟回读。
- **4.2** `src/dev/sdcard.{h,cpp}`：CMD0/CMD8/ACMD41/CMD2/CMD3/CMD9/CMD7/CMD13 + 响应类型 + CID/CSD/SCR/OCR；镜像打开/读扇区。
- **4.3** 数据通路：CMD16/17/18/23/24/25/12 + SDMA/ADMA2 → `AXI32_RAM`。
- **4.4** SoC 接线（`Sdhci.irq → intc.input[2]`）+ `main.cpp` 加 `--sdcard <img>` + 造 `sdcard.img`（**MBR 分区表 + 单个 ext2 分区**，见 §10 已定 8、9）：
  - `truncate -s 16M sdcard.img`（1MiB 对齐区 + ext2 分区）
  - `printf 'label: dos\n2048,,83\n' | sfdisk sdcard.img`（写 MBR：分区 1 起始 LBA 2048、类型 `0x83`）
  - `mke2fs -t ext2 -d <rootfs目录> part.img <块数>`（造出分区内容）
  - `dd if=part.img of=sdcard.img bs=1M seek=1 conv=notrunc`（写入 1MiB 偏移处）
  - 全程**不需要 root**；`sfdisk -l sdcard.img` 验证分区表
- **4.5** **内核验收**：用 `catnipsoc_sd_defconfig` 编核 → 启动 → `mmc0: new SDHC card` → `lsblk`（要同时看到 `mmcblk0` 与 `mmcblk0p1`，后者出现即"分区扫描通了"）→ `dd` / `mount /dev/mmcblk0p1`。
- **4.6**（可选）4-bit/高速、写回、多块与性能。

### 阶段 5：收尾

- 文档（本文件 §11 进度、`uart_notes.txt` 或新建 `net_notes.md`/`sd_notes.md` 的笔记体例）、`make help`、`tb/Makefile help`。

---

## 8. 自测与回归

- **设备自测**（无内核、毫秒级，遵循 `tb/uart_test.cpp` 的独立链接方式）：
  - `tb/eth_test`：按驱动同样的寄存器操作顺序走一遍 TX/RX/MAC 编程/MDIO，断言每一步状态位。
  - `tb/intc_test`（阶段 3）：置输入→查 IPR/IVR→写 IAR 清→查 MER 读回。
  - `tb/sd_test`（阶段 4）：枚举到 CMD9/CMD7，再单块读一个已知扇区比对。
- **手工回归**：每步之后 `make && make run -n` 之外要真起一次 u-boot/内核（阶段 0/1 尤其重要，确认没有把 SoC 启动搞坏）。
- **联调验收**：u-boot `ping`/`tftp`（阶段 2）；Linux `ping`/`ssh`（阶段 3）；`mmcblk0p1`（阶段 4）。
- 调试手段：已有 `mips32_tracer`；设备内定点日志用编译开关控制（默认关）。

---

## 9. 风险与对策

| 风险 | 说明 | 对策 |
|---|---|---|
| TX 完成位语义 | §2.4 表里的"清 BUSY 留 ACTIVE"，最易踩 | 1.1/1.6 的自测专门断言；u-boot `ping` 会二次暴露 |
| Linux RX 只有中断路径 | 没有 INTC 则永远收不到包 | 阶段 2 先用 u-boot 轮询验证；阶段 3 才引入 INTC |
| INTC 中断风暴 | `IVR` 返回非 0xFFFFFFFF、IAR 清了却自己重挂、MER 读不回都会出问题 | `tb/intc_test` 覆盖；一旦风暴，先查 IVR/IAR |
| PHY 中断导致状态机不轮询 | DT 给 `phy0` 分了 INTC 输入 0（边沿型）；模型从不拉中断时链路可能一直不上 | 在 link up 时置一次 `BMSR` link-change 锁存位并拉一次边沿；退路是让锁存位可读清、驱动自行走到 polling |
| 后端依赖/权限 | `libslirp-dev` 本机未安装（无 `.pc`/头文件）；`tap` 需 root | 装依赖前先问你（§10 已记录）；`tap` 暂不做 |
| slirp 的 ICMP 边界 | 客机 `ping` 公网不通（ICMP echo 出网需特权），只有 `ping 10.0.2.2` 通 | 验收统一按 `ping 10.0.2.2` 写；真需要 ICMP 出网时才考虑 `tap` |
| M3 的 ssh 验收前提 | 宿主当前没有 sshd 在监听 22（实测） | 步骤 3.3 开工前先确认宿主的 sshd 可用 |
| SDHCI 挑剔 | sdhci core 校验 caps/时钟/超时字段，可能反复 | 4.1 先把寄存器骨架与时钟回读做对再进卡协议；参考 QEMU 实现 |
| 内核 SD 版本差异 | 默认 `.config` 关 MMC，要用 `catnipsoc_sd_defconfig` | 4.5 明确用 sd defconfig 编核，并固定该产物路径 |
| `sdcard.img` 不存在 | 工作区里没有这个文件 | 4.4 里先造一个（内容方案见 §10） |

---

## 10. 决策记录与待定项

**已定**
1. SD 走**路线 a**（LogiSDHC/SDHCI + `sdcard.img`），不做 SD-over-SPI。
2. 网络与 SD 都**不改内核/u-boot 的 DTS 与 defconfig**（SD 只换 defconfig 编核，不是改配置项）。
3. 先网络、后 SD；网络内部先轮询（u-boot）验证、后中断（Linux）。
4. 命名用 `src/dev/ethernetlite.{h,cpp}` / `class EthernetLite`（忠于 Xilinx IP 名 `xps-ethernetlite`；"lite" 后缀在这里是 IP 官方名的一部分）。INTC 用 `src/dev/xps_intc.*` / `class XpsIntc`，SD 用 `src/dev/sdhci.*` + `src/dev/sdcard.*`。
5. **第一个后端 = `slirp`（2026-09-21 定）**。理由：M2 验收含 `tftpboot` 取文件，而 libslirp 自带 TFTP 服务（宿主免装 tftpd）；`socket` 需要自写帧层对端（成本最高）；`tap` 需要 root + 改宿主网络（本机 VirtualBox 下桥接不可靠）。`null` 仍用于 `tb/eth_test` 自测，`socket`/`tap` 留作将来可选后端。前置：`sudo apt install libslirp-dev`（动手前先问）。部署口径见 §6。
6. **后端代码放 `src/net/`（2026-09-21 定）**：`Makefile` 的 `VPATH` 与 `CXXFLAGS` 的 `-I` 各加一项（`src/net`）；对应步骤 2.1。
7. **SD 的 DMA 直接传指针（2026-09-21 定）**：`Sdhci` 持有一个 `AXI32_Slave*`（bus 或 memory）作为 DMA 目标，SDMA/ADMA2 同步读写它，不抽额外接口；对应步骤 4.3。
8. **`sdcard.img` 用 ext2（2026-09-21 定）**：`catnipsoc_sd_defconfig` 里 `CONFIG_EXT2_FS=y`（1435 行）而 `# CONFIG_EXT4_FS is not set`（1438 行）→ **ext2 零改动可用；ext4 必须破"不改 defconfig"约束**（还要连带 `JBD2`/`MBCACHE`）。ext2 无日志，在"只读 / 写回不完整"的 SD 模型下比 ext4 更稳。分区形态已定（见第 9 条：带 MBR 分区表 + 一个 ext2 分区）；造镜像的命令见步骤 4.4。

9. **`sdcard.img` 带 MBR 分区表 + 一个 ext2 分区（2026-09-21 定）**：即原 B 方案。理由：给"从 SD 启动"（见下「已登记」）留路——主流嵌入式板子的工具链与启动命令都按"设备:分区"寻址，u-boot 在有分区表时可以用标准写法 `mmc 0:1`（无分区表则只能退化成 `mmc 0` / `mmc 0:auto`，见 `disk/part.c:556-577`）；成本只是多两条造镜像命令，且**不需要 root**。布局：扇区 0 = MBR（分区项 1：类型 `0x83`，起始 LBA 2048 即 1MiB 偏移），从 1MiB 起是 ext2 文件系统；镜像总大小 = 1MiB + 分区大小。造法见步骤 4.4。

**待定（需要你拍板）**
截至 2026-09-21，**全部已定、无未决项**：
1. ~~第一个真实后端~~ → `slirp`（「已定」5）
2. ~~后端代码目录~~ → `src/net/`（「已定」6）
3. ~~DMA 访问内存的方式~~ → 直接传指针（「已定」7）
4. ~~`sdcard.img` 的内容与分区形态~~ → **ext2、带 MBR 分区表**（「已定」8、9）

**已登记、暂不排期**

> 下面两条是"完整地从 SD 启动"的两半，天然配套；都**不要在 M5 里做**。

- **目标 A：u-boot 从 SD 加载 kernel**（2026-09-21 登记，排在 **M5 之后**）。动机：真机上除了网口 TFTP 没有别的方式喂 kernel（真机现存 `configs/catnipsoc_tftp_defconfig` 正是这个现状）；SPI flash 虽然能零驱动启动，但烧写要靠 Xilinx/JTAG 工具、迭代不便；**SD 卡可插拔可写，是唯一灵活的载体**。
  - 依赖：M5 的 SDHCI 模型先做好（模拟器侧可以先行验证 u-boot 驱动，不必等真机）。
  - 真机侧工作：u-boot defconfig 开 `MMC`（或 `MMC_SDHCI`）/`DM_MMC`/`CMD_MMC`/`CMD_PART`/`FS_EXT4`(+`CMD_EXT4`/`FS_GENERIC`)；u-boot 的 DTS 加 `mmc@10300000`（`arasan,sdhci-8.9a`）；写一层 SDHCI glue 驱动（upstream u-boot 有通用 `drivers/mmc/sdhci.c`，LogiSDHC 是 SDHCI 兼容 → 薄 glue）。
  - **明确破例**：这一步需要改 u-boot 的 defconfig 与 DTS —— 本项目第一次破"不改 u-boot 配置"的约束（只此一处；**内核侧仍然不动**）。
  - 启动命令形如：`ext4load mmc 0:1 0x81000000 uImage; bootm 0x81000000`。
- **目标 B：把 SD 上的 ext2 当根文件系统**（2026-09-21 登记，用户明确排在 **M6 之后**）。ext2 驱动已在 defconfig 里（`CONFIG_EXT2_FS=y`），**不需要改 defconfig**。两条实现路径——① 改 bootargs 为 `root=/dev/mmcblk0p1 rootfstype=ext2 rootwait rw`（落在 DTS 的 `chosen/bootargs` 或 u-boot 环境里，会碰"不改 DTS"，需单独评估）；② 保留 initramfs，让 initramfs 的 init 挂载 SD 后 `switch_root`（**不碰 bootargs/DTS**，改动只在 initramfs 的内容里）。镜像里需要一套可用的 rootfs 内容（可复用现有 initramfs 的）。

**备查：SD-over-SPI（路线 b，未选）**
真机 0x1D000000 的 AXI Quad SPI 是 AXI4 memory-mapped 的 SPI flash（XIP）窗口，控制口没接进 CPU 地址映射；要真做 SD-over-SPI 得自造 SPI 控制器接法 + 改两份 DTS + 在内核/u-boot 开 `CONFIG_SPI`/`CONFIG_MMC_SPI` + 实现 SD SPI 模式协议（更重且偏离真机）。

---

## 11. 进度

| 步骤 | 状态 | 备注 |
|---|---|---|
| 0.1 ethernetlite 骨架 | ☐ 未开始 | |
| 0.2 SoC 常量/挂载 | ☐ 未开始 | |
| 1.1 TX 路径 | ☐ 未开始 | |
| 1.2 MAC 编程 | ☐ 未开始 | |
| 1.3 RX 路径 | ☐ 未开始 | |
| 1.4 中断条件 | ☐ 未开始 | |
| 1.5 MDIO + PHY 桩 | ☐ 未开始 | |
| 1.6 tb/eth_test | ☐ 未开始 | |
| 2.1 backend 接口 + null | ☐ 未开始 | §10.2 已定：`src/net/`（VPATH 与 -I 各加一项） |
| 2.2 slirp 后端 | ☐ 未开始 | 已定 §10：slirp；开工前需装 `libslirp-dev` |
| 2.3 main.cpp `--net` | ☐ 未开始 | |
| 2.4 u-boot ping/tftp 验收 | ☐ 未开始 | 关键去风险点 |
| 3.1 XpsIntc + 自测 | ☐ 未开始 | |
| 3.2 SoC 中断接线 | ☐ 未开始 | |
| 3.3 Linux ping/ssh 验收 | ☐ 未开始 | |
| 3.4 hostfwd/--mac/文档 | ☐ 未开始 | 可选 |
| 4.1 Sdhci 骨架 + 自测 | ☐ 未开始 | |
| 4.2 SdCard 命令集 | ☐ 未开始 | |
| 4.3 数据通路 + DMA | ☐ 未开始 | §10.3 已定：直接传 `AXI32_Slave*` |
| 4.4 `--sdcard` + 造镜像 | ☐ 未开始 | §10 已定 8/9：ext2、MBR + 单分区 |
| 4.5 内核 mmcblk0 验收 | ☐ 未开始 | 验收 `mount /dev/mmcblk0p1` |
| 4.6 可选项 | ☐ 未开始 | |
| 5 收尾与文档 | ☐ 未开始 | |
