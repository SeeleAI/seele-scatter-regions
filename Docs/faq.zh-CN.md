# 常见问题

这里回答关于 Seele Scatter Regions 的常见问题。它是一个面向 Unreal Engine 5
的开源程序化场景与世界构建插件。

## Seele Scatter Regions 能做什么？

它可以在 Unreal Editor 中生成带随机种子的村庄、农场和墓地区域。区域可包含
道路、边界、建筑、农田、墓碑及其他道具；可复用的配方资产决定使用哪些
Static Mesh 以及它们的放置密度。

## 支持哪些 Unreal Engine 版本？

0.1.1 版本以 Unreal Engine 5.8 为目标版本，并已在该版本完成验证。Unreal
Engine 5.5 用户可使用 0.1.0；其他版本目前不在已验证兼容范围内。

## 是否依赖 Unreal Engine 的 PCG 框架？

不依赖。插件实现了独立的配方驱动生成器，不依赖 Unreal Engine PCG 插件。
本项目中的“程序化生成”描述的是工作流，而不是对 PCG 框架的依赖。

## 能否在游戏运行时生成？

0.1.1 暂不支持。公开生成器位于 Unreal Editor 模块中。生成的 Actor 和
`UInstancedStaticMeshComponent` 结果可以随关卡保存，但仓库当前没有提供
打包游戏运行时生成接口。

## 是否必须使用 C++ Unreal 项目？

是。本项目是包含 Runtime 与 Editor C++ 模块的源码插件。请将它安装到 C++
Unreal 项目中，启用插件后重新编译项目。

## 能否通过 Blueprint 或自动化调用？

可以，但调用发生在编辑器中。插件提供 Blueprint 可调用的 Editor Subsystem、
C++ 生成器和 JSON 适配器。请参阅[生成 API](generation-api.md)和可编辑的
[示例请求](../Samples/CommandPayloads)。

## 同一个随机种子能否得到可重复结果？

可以。在配方、随机种子、输入参数及相关场景状态相同的情况下，放置结果可以
复现。修改配方、输入参数、Landscape 或射线检测涉及的场景状态可能改变结果。

## 是否必须有 Landscape？

插件支持 Landscape 投影，并返回投影命中与未命中数量。如果没有命中
Landscape，生成器可以保留配方的局部高度并返回警告；接受结果前应检查结构化
返回值。

## 仓库是否包含预览图中的美术资产？

不包含。预览中的 Mesh、材质和贴图不会随仓库分发。仓库提供基于 Unreal
Engine Basic Shapes 的轻量配方用于安装验证；之后可以复制配方并换成自己的
资产。

## 当前有哪些区域类型？

0.1.1 支持 `Village`、`Farm` 和 `Cemetery`。欢迎在
[GitHub Discussions](https://github.com/SeeleAI/seele-scatter-regions/discussions)
提出新的区域需求和方案。

## 插件是否免费开源？

是。源码采用 [MIT License](../LICENSE)，归属及声明信息见
[NOTICE.md](../NOTICE.md)。

## 在哪里报告问题？

请创建 [GitHub Issue](https://github.com/SeeleAI/seele-scatter-regions/issues)，
并附上 Unreal Engine 版本、区域类型、随机种子、配方资产路径，以及返回的
警告或错误。

---

[English FAQ](faq.md) · [简体中文 README](../README.zh-CN.md)
