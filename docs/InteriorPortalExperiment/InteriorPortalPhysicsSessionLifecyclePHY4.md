# PHY-4 — 原生通行会话的生命周期

状态：**隔离 Chaos 夹具验证通过；生产绑定和 Core 验收仍开放**（2026-09-29）。前置 GT 绑定准备提交为 `2e1b3e4`。

## 所有权

一个 `FChaosPassageSession` 对应一个刚体身份、配对代次、solver epoch、binding epoch 和两侧门框。它拥有通行协调器、唯一的原生传送写入器和可选的原生静态许可器。生产绑定必须先从实际组件准备请求、安装当前刚体的原生速度上限，并将当前三个 native proxy 及准备结果交给同一会话；没有成功附着许可器，不得提交穿墙/传送许可。现有测试夹具还保留无原生许可器的早期隔离协议情形，不能将这些情形当作生产模式。

配对或刚体身份改变、许可器发现 native 绑定改变、任一绑定 proxy 注销，都沿同一 `Retire_Internal` 路径永久撤销当前许可、传送写入和许可器。退役返回带旧 body/solver/binding 身份的待消费 `FTransferFact` 值拷贝。外层所有者必须先接管这些已提交事实，再销毁会话或旧 proxy；不得因最新输入已取消而丢弃或在新 binding 内重放旧事实。重复退役不会重新激活会话，正确域内的确认仍可清空旧 journal。新配对或重建的 proxy 必须建立新 binding epoch 与新会话；不能修改旧会话的身份继续使用。

该类型只管理物理线程资源的共同生死，不负责跨 GT/PT 的生产注册表、`UObject` 生命周期或世界关闭时的事实队列。旧关卡的 `PhysicsHandle`、关节仲裁、常规 Recovery 和后置身体位置写入仍在原玩法路径中；新会话只接入 Editor Chaos 夹具，不能与旧写入一起挂到实际地图。

## 验证和下一步

UE 5.8 捆绑 .NET 工程生成及 Development Editor 编译 PASS：`Saved/Logs/PortalPhysicsPHY4SessionFinalProjectFiles.log`、`Saved/Logs/PortalPhysicsPHY4SessionFinalBuild.log`。`SlayTheSpireDemo.Interior.Portals.Physics` 在 2026-09-28 17:20 UTC **44/44 通过、0 失败、1 项含 2 条既有警告**：`Saved/AutomationReports/PortalPhysicsPHY4SessionDiagnostic/index.json`、`Saved/Logs/PortalPhysicsPHY4SessionDiagnosticAutomation.log`。警告属于未修改的 `PhysicsFoundation.GenerationAndSharedQuery`；新增 `PhysicsSolverClearance.RetiredFactHandoff` 为 1/1 通过、0 警告。

新增测试在传送事实尚未消费时取消配对，确认许可和后续写入立即退役、旧事实以旧身份交接且只消费一次，确认后 journal 排空而旧 binding 不复活。已有物理回归覆盖 native proxy 注销、body 重建及许可器识别绑定变化。本项没有关卡 PIE；地图 SHA-256 保持 `0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`。

下一项是生产 GT/PT 绑定所有者：管理新旧会话交替、held/free 输入、每子步许可、事实与确认的异步交接，并证明旧会话销毁前不会遗失事实。随后才可在一个切换点移除旧物理写入，执行实际地图 Core 自动化与人工 PIE。当前尚不能宣称方块抽动已修复。
