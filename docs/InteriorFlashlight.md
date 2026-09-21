# 小屋角色手电筒

角色 `InteriorChildCharacter` 自带手电筒，默认关闭，F 每次按下切换一次，HUD 显示当前状态。Q 昼夜切换和墙上灯具开关不控制手电筒。

原生组件 `FlashlightRig` 挂在第一人称摄像机上，统一承载筒身、金属灯头、镜片、简易持握手部与聚光灯。光源在镜片前方 0.3 cm，沿筒身轴线发射；不是屏幕中心的照明贴片。基础形体使用引擎圆柱/球体与 House 已有材质，没有精细手指骨骼或持握动画。

聚光灯为 700 lm、14 m 衰减半径、12° 内锥角/28° 外锥角、暖白色，启用动态阴影与低强度体积雾散射。光柱依赖场景已有体积雾，不额外提高整张地图的雾密度或改动曝光。

`/Game/House/Flashlight/M_LF_Flashlight` 是程序化灰度 Light Function：中心亮区、外围渐暗、微弱同心纹路和不均匀性。材质仅使用 UV 运算，显式兼容 Light Function Atlas。

角色 Details/子蓝图可调整：

- `FlashlightStartsOn`：是否默认开启。
- `FlashlightSwayAmount`：默认 0.5；设为 0 关闭呼吸/行走摆动。
- `FlashlightFollowSpeed`：默认 14；值越大，转向跟随越快。滞后限制在约 4°，避免明显偏离视线。
- `Flashlight` 组件中的亮度、颜色、衰减半径、锥角、阴影和散射设置。

每帧从摄像机向灯头做半径 4 cm 的碰撞扫描，遇到阻挡时回收整组组件，保证光源仍在镜片前方。场景物体需阻挡 Visibility 通道才参与此检测；若命中已放置传送门开口内的支撑墙，则视为传送门通道，不回收手电筒。

## 验证

AUTOMATED GATES：UE 5.8 Development Editor 构建通过；`SlayTheSpireDemo.Interior.Flashlight` 通过，覆盖唯一 F Press 绑定、绑定调用开关、幂等开启、无昼夜控制器时独立工作与光斑材质引用。PIE 断言通过：默认关闭、开灯可见、镜片与光源间距 0.3 cm、共同父组件。

最终 PIE 夜间光斑已检查（`Saved/FlashlightFinal.png`）。靠墙运行时断言通过：Rig 回收 X=-13.62 cm，光源 Y=-398.29 cm 保持在墙内侧，和镜片间距仍为 0.3 cm。退出 PIE 后恢复临时后台性能设置，地图原灯具状态不受测试影响。

MANUAL PIE GATES — USER ACTION REQUIRED：在 `/Game/House/L_Interior_LivingKitchen` 点击游戏视口获取焦点，按 Q 到夜晚，再按两次 F，预期光束与 HUD 的 ON/OFF 同步切换；转身、行走及靠墙，检查手持摆动与回收手感。再用 LMB/RMB 在可放置墙面建立一对传送门，站到传送门开口前，预期手电筒保持图一的长度和位置，不因传送门后方支撑墙命中而收回；将视线移到开口外的普通墙面，预期仍正常收回。实际桌面按键注入未可靠触发，本次只将 F 绑定自动化通过与运行时组件断言记为通过，不宣称实际键盘或持握手感已人工验收。
