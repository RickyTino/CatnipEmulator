# catnip_emulator 外设扩展：概览与进度（给人看）

> 这份是**给你（RickyTino）看**的概览与进度看板。
> 可执行的技术细节在 [impl_plan.md](impl_plan.md)（给 agent 的底稿）：驱动语义清单、寄存器规格、微步骤与验收命令。本文件不重复那些内容，只在需要时指到它的章节号。
> 更新约定：每推进一步就更新本文件 §4 进度；里程碑或决策有变时更新 §3、§5。

---

## 1. 我们做什么，为什么这么做

**现状**：`cemu` 目前只建模了 CPU + 128MB 内存 + bootrom + UART16550，能真跑 u-boot 与 Linux 内核。

**阶段一（网口）**：把该有的网口补上——AXI 中断控制器（0x10200000）+ Xilinx `xps-ethernetlite`（0x10E00000）+ MDIO/PHY 桩，再给它接一个宿主网络后端（**已定 `slirp`**，见 §5 第 1 项）。目标是你电脑上的 Linux 能 `ping` 通宿主、并 `ssh`/`telnet` 过去。

**阶段二（SD 卡，路线 a）**：按真机建模 LogiSDHC（0x10300000，Arasan SDHCI 兼容的 SD 主机控制器）+ SD 卡协议模型 + `sdcard.img` 落盘。目标是内核认到块设备 `/dev/mmcblk0` 及其分区 `/dev/mmcblk0p1`，能 `dd` / `mount`。

几个关键取舍（理由都在 impl_plan.md §2、§10）：

- **SD 不走 SD-over-SPI**：真机 0x1D000000 那个 AXI Quad SPI 是接到 SPI flash 的 XIP 窗口，不是 SD 通路；硬做要改硬件设计 + 两份 DTS + 内核 config，更重且不真。
- **不改内核/u-boot 的 DTS 与 defconfig**：真机上这两个外设的节点与驱动本来就齐了（SD 只是要换成 `catnipsoc_sd_defconfig` 编核）——**软件侧几乎零改动**是这条路线的最大红利。
- **先网络后 SD；网络内部先轮询（u-boot）后中断（Linux）**：u-boot 的网卡驱动是纯轮询、也不需要中断控制器，所以能先用它把 MAC 寄存器语义验对，再引入最麻烦的中断部分。

**改动边界**：不动 `src/bus/axi.{h,cpp}`、不动内核/u-boot 的 DTS 与 config（**限 M0–M5**；唯一例外见 §5"已登记"的目标 1）、不往 NSCSCC 那套测试里塞东西。

---

## 2. 真机现状一页

模拟器要照着实现的地址映射（来自 `CatnipSoC.xpr` 的 block design）：

| 基址 | 范围 | 实例 / IP | cemu 现状 |
|---|---|---|---|
| 0x00000000 | 128M | DDR 内存 | ✅ 已建模 |
| 0x10200000 | 64K | `axi_intc_0`（3 输入，级联到 CPU IP3） | ⬜ 未建模 |
| 0x10300000 | 64K | `logisdhc_0`（SD 主机，SDHCI 兼容） | ⬜ 未建模 |
| 0x10400000 | 64K | `axi_uart16550_0` | ✅ 已建模 |
| 0x10600000 | 64K | `axi_gpio_0` | ⬜ 不在本次范围 |
| 0x10E00000 | 64K | `axi_ethernetlite_0` + `mii_to_rmii` | ⬜ 未建模 |
| 0x1D000000 | 16M | AXI Quad SPI（接 SPI flash，XIP） | ⬜ 不在本次范围 |
| 0x1FC00000 | 1M | `axi_bram_ctrl_0`（bootrom） | ✅ 已建模 |

中断一句话：UART 直连 CPU IP2；MAC、PHY、SD 走 `axi_intc_0` 的输入 1/0/2，`axi_intc_0` 再级联到 CPU IP3。

软件就绪度：以太网节点/驱动两侧都齐；SD 节点与 `catnipsoc_sd_defconfig` 已就绪；**网络与 SD 都不需要改内核或 u-boot 的任何配置**（细节见 impl_plan.md §2.3）。SD 卡镜像 `sdcard.img` 目前不存在，阶段二第 4 步才需要造。

---

## 3. 里程碑与验收标准

| 里程碑 | 做完什么 | 怎么算通过（你能自己看的） | impl_plan 步骤 |
|---|---|---|---|
| M0 脚手架 | 建 ethernetlite 骨架、挂进 SoC（无行为） | `make` 通过；`make run` 起 u-boot/内核与之前一模一样 | 0.1–0.2 |
| M1 设备行为 + 自测 | MAC 收发、MAC 编程、中断条件、MDIO/PHY 桩，配一套寄存器级自测（不跑内核） | `make -C tb eth-test` 全绿 | 1.1–1.6 |
| **M2 网络打通（u-boot）** | 接 `slirp` 后端（libslirp） | **u-boot 提示符下 `ping 10.0.2.2` 与 `tftpboot` 取文件都通**（此时还不需要中断控制器）——这是整件事的去风险点 | 2.1–2.4 |
| M3 Linux 联通 | 加 AXI INTC 并把中断接好 | Linux 下 `ifconfig eth0 up` 显示 link up；`ping` 通；能 `ssh`/`telnet` 到宿主 | 3.1–3.3 |
| M4 打磨 | 反向端口转发、MAC 地址选项、文档 | 宿主能 `ssh` 进客机（`--hostfwd`） | 3.4（可选） |
| **M5 SD 打通** | SDHCI 主机 + SD 卡模型 + 镜像落盘 + 中断接线 | 内核日志出现 `mmc0: new SDHC card`，`lsblk` 看到 `mmcblk0p1`，`dd`/`mount /dev/mmcblk0p1` 成功 | 4.1–4.5 |
| M6 收尾 | 可选项、文档与 `make help` | — | 4.6、阶段 5 |

每个设备都自带一个"不跑内核"的秒级自测（`tb/eth_test`、`tb/intc_test`、`tb/sd_test`），所以大部分寄存器错误在进内核之前就会被抓住。

---

## 4. 进度

| 阶段 | 状态 | 备注 |
|---|---|---|
| M0 脚手架 | ⬜ 未开始 | 本次会话只产出方案，代码零改动 |
| M1 设备行为 + 自测 | ⬜ 未开始 | |
| M2 u-boot ping/tftp 验收 | ⬜ 未开始 | 后端已定 slirp（§5）；开工前需装 `libslirp-dev` |
| M3 Linux ping/ssh 验收 | ⬜ 未开始 | |
| M4 打磨（可选） | ⬜ 未开始 | |
| M5 SD（mmcblk0p1） | ⬜ 未开始 | |
| M6 收尾 | ⬜ 未开始 | |

细化到 23 个编号步骤的进度表在 impl_plan.md §11（agent 用）。

---

## 5. 需要你拍板的问题

> 截至 2026-09-21 **全部已定，无未决项**（下面保留原选项与结论备查）。

| # | 问题 | 选项 | 什么时候必须要定 | 我的倾向 |
|---|---|---|---|---|
| ~~1~~ | ~~第一个宿主网络后端~~ | ✅ **已定（2026-09-21）：`slirp`** —— libslirp 自带 TFTP 服务（宿主免装 tftpd）且免 root；`socket` 要自写帧层对端、`tap` 要 root + 改宿主网络。详见 impl_plan.md §6 / §10 | — | — |
| ~~2~~ | ~~后端代码放哪~~ | ✅ **已定（2026-09-21）：`src/net/`**（Makefile 的 `VPATH` 与 `-I` 各加一项） | — | — |
| ~~3~~ | ~~SD 的数据 DMA 怎么拿到内存~~ | ✅ **已定（2026-09-21）：直接传 `AXI32_Slave*`**，不抽接口 | — | — |
| ~~4~~ | ~~`sdcard.img` 的分区形态~~ | ✅ **已定（2026-09-21）：带 MBR 分区表 + 一个 ext2 分区（`mount /dev/mmcblk0p1`）** —— 为"从 SD 启动"留路，造镜像只多两条命令 | — | — |

**为什么 `sdcard.img` 用 ext2**：`catnipsoc_sd_defconfig` 里 `CONFIG_EXT2_FS=y` 而 `# CONFIG_EXT4_FS is not set` —— **ext2 零改动可用，ext4 要破"不改 defconfig"约束**（还要连带 `JBD2`）。另外 ext2 无日志，在"只读 / 写回不完整"的 SD 模型下比 ext4 更稳。顺手加 MBR 分区表主要是给"从 SD 启动"铺路：u-boot 按"设备:分区"寻址，有分区表就能用标准写法 `mmc 0:1`（没分区表只能退化成 `mmc 0`）。

**已登记、暂不排期**（"完整地从 SD 启动"的两半，**都不在 M5 里做**）：

1. **u-boot 从 SD 加载 kernel**（排在 **M5 之后**）—— 动机：真机上除了网口 TFTP 没有别的方式喂 kernel；SPI flash 能启动但要靠 Xilinx/JTAG 工具烧、不便迭代，SD 卡可插拔可写才是灵活的载体。代价：**要破一次"不改 u-boot DTS/defconfig"**（开 `MMC`/`DM_MMC`/`CMD_MMC`/`CMD_PART`/`FS_EXT4` + 加 `mmc@10300000` 节点 + 一层薄 SDHCI glue 驱动）。
2. **把 SD 上的 ext2 当根文件系统**（排在 **M6 之后**）—— ext2 驱动已在 defconfig 里，不用改配置；两条路：① 改 bootargs 从 SD 引导（碰"不改 DTS"），② 保留 initramfs 用 `switch_root` 切到 SD（不碰 DTS/bootargs）。

细节见 impl_plan.md §10。

已定的决策（不用再讨论）：SD 走路线 a；网络与 SD 都不改 DTS/defconfig（**唯一例外**：将来"u-boot 从 SD 加载 kernel"需破一次 u-boot 侧）；先网络后 SD；命名用 `src/dev/ethernetlite.*`（INTC 用 `xps_intc.*`，SD 用 `sdhci.*` + `sdcard.*`）；第一个后端 = `slirp`；后端代码放 `src/net/`；SD 的 DMA 直接传 `AXI32_Slave*`；`sdcard.img` = **ext2 + MBR 分区表（`mount /dev/mmcblk0p1`）**。

---

## 6. 术语速查

| 词 | 一句话 |
|---|---|
| `xps-ethernetlite` | Xilinx 的 10/100M 网卡 IP（很轻量：只有收发缓冲 + 几个寄存器）；Linux 驱动叫 `emaclite` |
| ping-pong | 收/发各有两个缓冲，硬件轮流用；模型必须两个都实现（DT 里都开着） |
| MDIO / PHY | 网卡的"管理接口"与外围 PHY 芯片；这里要做一个假 PHY，让驱动认为"网线已插好、链路 up" |
| AXI INTC | Xilinx 的中断汇聚控制器；MAC/PHY/SD 的中断先进它，再进 CPU |
| LogiSDHC / SDHCI / Arasan | 真机上那颗 SD 主机控制器，寄存器兼容"SDHCI"标准（Arasan 是标准的一种实现），所以内核用 `sdhci-of-arasan` 驱动 |
| SDMA / ADMA2 | SDHCI 的两种 DMA 方式（把卡上数据搬到内存）；模型里是同步完成，不涉时序 |
| `sdcard.img` | 一个原始磁盘镜像文件（**MBR 分区表 + 一个 ext2 分区**），模拟器把它当"插进去的 SD 卡"内容 |
| MBR 分区表 | 磁盘最前面 512 字节里的分区表（4 个主分区项 + `55 AA` 签名）；内核读它就把 `mmcblk0` 切成 `mmcblk0p1` |
| slirp / TAP | 两种把客机接进宿主网络的方式：slirp 是用户态 NAT（免 root，客机走 NAT 出去，**本方案已选**）；TAP 是给客机一张真实的二层网卡（需要 root，留作将来可选） |
| 内置 TFTP 服务 | libslirp 自带的一个 TFTP 服务器，地址就是虚拟网关 `10.0.2.2`；u-boot 的 `tftpboot` 靠它取文件，宿主无需装 tftpd |
| u-boot / `bootm` | 客机上的引导程序；`make run` 会在 u-boot 提示符停下，敲 `bootm 0x81000000` 才引导内核 |

---

## 7. 你想自己看一眼时怎么做

| 想验证 | 命令 |
|---|---|
| 现状（没动任何外设）能跑 | `cd catnip_emulator && make run`（u-boot + 内核）或 `make run_linux`（直接起内核） |
| 寄存器级自测（不跑内核，秒级） | `make && make -C tb eth-test`（M1 之后可用；之后还有 `intc-test` / `sd-test`） |
| M2 之后：网络通不通 | `make run` → u-boot 里 `dhcp` → `ping 10.0.2.2`；取文件 `tftpboot 0x81000000 <文件名>` |
| M3 之后：Linux 网络 | 进 Linux 后 `ifconfig eth0 up`、`ping 10.0.2.2`、`ssh 10.0.2.2` |
| M5 之后：SD 卡 | 进 Linux 后 `lsblk`（看 `mmcblk0` + `mmcblk0p1`）、`dd if=/dev/mmcblk0p1 ...`、`mount /dev/mmcblk0p1` |

---

## 8. 我们怎么配合（每步的节奏）

每推进一步，我会给你三样东西，**默认都很简短**：

1. **改了什么** —— 文件与要点清单
2. **为什么这么做** —— 一两句：这一步解决什么问题、为什么用这个做法（优先点出驱动/硬件依据里的坑）
3. **验证证据** —— 真实命令与输出；不做"应该没问题"的结论

需要更详细的推导、取舍或背景时，你追问，我再展开。这条约定也写在 impl_plan.md §0。
