# catnip_emulator 开发约定

外设扩展（网口 / SD）的工作由两份文档驱动，**动手前先读**：

- `impl_plan.md` —— 技术底稿：寄存器规格（含驱动 file:line 依据）、地址与中断拓扑、编号微步骤与每步的验收命令、风险、§10 决策记录、§11 进度表
- `plan_overview.md` —— 概览与进度看板：目标与理由、里程碑与验收标准、当前决策、术语表

## 工作方式

- 一次只做一个编号步骤（`impl_plan.md` §7）；动手前说明「改哪些文件 / 为什么 / 怎么验证」
- 每步都要跑出真实命令与输出，不做"应该没问题"的结论
- 不擅自扩大范围：不改内核/u-boot 的 DTS 与 defconfig —— **除非** `impl_plan.md` §10 明确登记为例外（目前只有"u-boot 从 SD 加载 kernel"这一条）
- 决策有变：先改 `impl_plan.md` §10/§11 与 `plan_overview.md` §4，再动手
- 提交信息（commit message）用英文

## 构建与运行

- `make` → 产出 `cemu` + `tb/nscscc_tests` + `tb/uart_test`
- `make run` = u-boot + 内核（停在 u-boot 提示符）；`make run_linux` = 直接起内核
- 设备自测（不跑内核、秒级）：`make -C tb eth-test` / `intc-test` / `sd-test`
- 运行依赖的镜像在同一工作区：`../u-boot-catnipsoc/u-boot.bin`、`../linux-5.6.14-catnipsoc/arch/mips/boot/uImage.bin`
