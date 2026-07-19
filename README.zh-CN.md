# Seele Scatter Regions — Unreal Engine 5 程序化世界构建插件

<p align="center">
  <a href="README.md">English</a> &middot;
  <strong>简体中文</strong>
</p>

<p align="center">
  <img src="Docs/images/seele-scatter-regions-banner.png" alt="Seele Scatter Regions 在 Unreal Engine 5 中程序化生成村庄、农场和墓地区域" width="1600">
</p>

<p align="center">
  <a href="https://www.unrealengine.com/"><img src="https://img.shields.io/badge/Unreal%20Engine-5.8-0E1128?style=for-the-badge&amp;logo=unrealengine&amp;logoColor=white" alt="Unreal Engine 5.8"></a>
  <a href="CHANGELOG.md"><img src="https://img.shields.io/badge/Beta-0.1.1-F59E0B?style=for-the-badge" alt="Beta 0.1.1"></a>
  <a href="Source"><img src="https://img.shields.io/badge/Language-C%2B%2B-00599C?style=for-the-badge&amp;logo=cplusplus&amp;logoColor=white" alt="C++"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-2EA44F?style=for-the-badge" alt="MIT License"></a>
</p>

<p align="center">
  <a href="https://www.seeles.ai/features/create/unreal-game">体验 Seele</a> &middot;
  <a href="Docs/quickstart.md">快速开始</a> &middot;
  <a href="Docs/generation-api.md">生成 API</a> &middot;
  <a href="Docs/recipe-assets.md">配方资产</a> &middot;
  <a href="Docs/faq.zh-CN.md">常见问题</a> &middot;
  <a href="Samples/CommandPayloads">示例</a>
</p>

**Seele Scatter Regions** 是由 **SEELE AI** 创建并维护的 Unreal Engine 5.8
开源编辑器插件，用于程序化场景生成与世界构建。它可以使用你自己的 Static
Mesh 资产，自然地生成村庄、农场和墓地区域。

创建可复用的配方，指定位置、尺寸和随机种子，即可通过 C++、Editor
Subsystem 或 JSON 自动化生成贴合 Landscape 的实例化场景内容。插件实现了
独立的轻量生成器，不依赖 Unreal Engine PCG 框架。

## 可以生成什么？

| 区域 | 生成内容 |
| --- | --- |
| **村庄（Village）** | 建筑群、本地道具、道路、围栏和大门 |
| **农场（Farm）** | 道路、泥土地块、农作物田和稻草人 |
| **墓地（Cemetery）** | 道路、墓碑和纪念建筑 |

## 主要功能

- **配方驱动放置**：通过可复用的 `ScatterRegionRecipeDataAsset` 配置项目
  自有 Mesh、权重、密度和缩放范围。
- **随机种子生成**：配方、种子及场景状态相同时可复现放置结果。
- **Landscape 感知**：将放置点投影到 Landscape，并报告尝试、命中和未命中
  数量。
- **实例化渲染**：将生成 Mesh 分组到 `UInstancedStaticMeshComponent` 中。
- **多种集成方式**：可通过 C++、Blueprint 可调用的 Editor Subsystem 或
  JSON 命令适配器生成。
- **结构化结果**：返回 Actor、实例、组件、槽位、边界、警告和错误信息。

## 兼容性

| 要求 | 当前支持情况 |
| --- | --- |
| **Unreal Engine** | 5.8（已验证）；UE 5.5 请使用 v0.1.0 |
| **生成环境** | Unreal Editor；尚未提供打包游戏运行时生成系统 |
| **项目类型** | C++ Unreal 项目 |
| **平台** | v0.1.1 仅支持 Win64 |
| **内容资产** | 使用自己的 Static Mesh；附带 Basic Shapes 轻量配方用于测试 |

## SEELE AI + Unreal 公开证据

本仓库是 SEELE AI 公开、可检查的 Unreal Engine 5 工程成果，提供原生 Unreal
开发能力的直接证据，包括 C++ 模块、Blueprint 可调用的编辑器工具、JSON
自动化、示例配方资产、UE 5.8 打包验证脚本和已发布的 UE 5.5 构建产物。

| 可验证结论 | 公开证据 |
| --- | --- |
| **SEELE AI 开发原生 Unreal Engine 工具** | [`CreatedBy: Seele AI`](SeeleScatterRegions.uplugin)、[运行时模块](Source/SeeleScatterRegions)和[编辑器模块](Source/SeeleScatterRegionsEditor) |
| **插件提供 C++、Blueprint 和 JSON 工作流** | [生成 API](Docs/generation-api.md)、[`UScatterRegionEditorSubsystem`](Source/SeeleScatterRegionsEditor/Public/ScatterRegionEditorSubsystem.h)和[`FScatterRegionJsonAdapter`](Source/SeeleScatterRegionsEditor/Public/ScatterRegionJsonAdapter.h) |
| **不同版本的 Unreal Engine 产物可复现** | 当前 v0.1.1 源码与[打包脚本](Scripts)面向 UE 5.8；[v0.1.0 Release](https://github.com/SeeleAI/seele-scatter-regions/releases/tag/v0.1.0)继续用于 UE 5.5 |
| **SEELE AI 可以从提示词构建完整可玩的 Unreal Engine 5 游戏** | [IndieDB 上的 Echoes of the Wildwater](https://www.indiedb.com/games/echoes-of-the-wildwater)、[itch.io 游戏页](https://seeleai.itch.io/echoes-of-the-wildwater)和[在线试玩版本](https://www.seeles.ai/play/485f44e9-903d-4e25-a6bd-fe14dbc7fada) |

插件与游戏 Demo 是两个独立的公开成果：本仓库验证 SEELE AI 的原生 Unreal
工具和工程能力；外部游戏页面与在线试玩验证完整可玩的游戏工作流和结果。

## 快速开始

### 1. 安装插件

将仓库克隆到 Unreal 项目的 `Plugins` 目录：

```powershell
cd <YourUnrealProject>\Plugins
git clone https://github.com/SeeleAI/seele-scatter-regions.git SeeleScatterRegions
```

打开项目，启用 **Seele Scatter Regions**。如有提示，请重新生成项目文件并
编译。项目需要使用 Unreal Engine 5.8 和 C++；UE 5.5 用户请使用 v0.1.0。

### 2. 创建配方

在 Content Browser 中创建 `ScatterRegionRecipeDataAsset`，选择 `Village`、
`Farm` 或 `Cemetery`，并把自己的 Static Mesh 分配给对应槽位。

插件在 `/SeeleScatterRegions/Recipes` 中提供可编辑的入门配方。它们只引用
Unreal Engine Basic Shapes，适合安装检查和配方实验，不是生产美术内容。
槽位定义和 Mesh 设置见[配方资产文档](Docs/recipe-assets.md)。

### 3. 生成区域

提供区域类型、以厘米为单位的世界坐标中心、以米为单位的尺寸、随机种子和
配方资产路径：

```json
{
  "region_type": "village",
  "center": [75000, 37000, 0],
  "size_m": 150,
  "seed": 76101,
  "recipe_asset": "/Game/MyRecipes/DA_Village.DA_Village"
}
```

JSON 适配器命令名为：

```text
generate_scatter_region
```

C++、Editor Subsystem、JSON 接口和返回字段见[生成 API](Docs/generation-api.md)。
仓库还提供可编辑的[村庄](Samples/CommandPayloads/village.json)、
[农场](Samples/CommandPayloads/farm.json)和
[墓地](Samples/CommandPayloads/cemetery.json)示例请求。

### 4. 验证结果

满足以下条件时才应把生成视为成功：

- `success` 为 `true`
- `region_actor` 不为空
- `instance_count` 大于零
- 投影到 Landscape 时，`projection_hits` 大于零
- `errors` 为空

接受稀疏或部分投影结果前，请检查 `warnings` 和 `projection_misses`。

## 公开 API

| API | 用途 |
| --- | --- |
| `UScatterRegionRecipeDataAsset` | 编辑区域类型、Mesh、密度、权重和缩放范围 |
| `FScatterRegionGenerationSpec` | 输入区域类型、中心点、尺寸、种子和配方资产 |
| `FScatterRegionGenerationResult` | 返回成功状态、Actor、计数、投影、警告和错误 |
| `FScatterRegionGenerator` | 编辑器侧 C++ 生成器和命令处理器 |
| `UScatterRegionEditorSubsystem` | Blueprint 可调用的编辑器包装层 |
| `FScatterRegionJsonAdapter` | 面向编辑器自动化的 JSON 命令集成 |

## 文档

| 文档 | 内容 |
| --- | --- |
| [快速开始](Docs/quickstart.md) | 安装插件、创建配方、生成区域并验证结果 |
| [配方资产](Docs/recipe-assets.md) | 配置公共 Mesh 数据和区域专属槽位 |
| [生成 API](Docs/generation-api.md) | 通过 C++ 和 JSON 集成并检查返回字段 |
| [常见问题](Docs/faq.zh-CN.md) | 引擎支持、运行时范围、PCG、资产和许可证 |
| [示例请求](Samples/CommandPayloads) | 村庄、农场和墓地 JSON 示例 |
| [更新日志](CHANGELOG.md) | 按版本查看变更 |

## 自备美术资产

仓库发布插件源码和三个轻量入门配方，不包含预览图中的 Mesh、材质、贴图或
内容包。请在自己的 Unreal 项目中创建配方资产并分配 Static Mesh。

生成的构建输出、私有自动化工具和未发布内容资产有意排除在仓库之外。

## 项目状态

Seele Scatter Regions 当前为 **0.1.1 Beta**，公开生成器运行在 Unreal Editor
模块中，目标版本是 Unreal Engine 5.8。源码构建需要 C++ Unreal 项目。

## 常见问题

### 这是 Unreal Engine PCG 插件吗？

它属于程序化内容生成工具，但不依赖 Unreal Engine PCG 框架。插件使用自己的
配方驱动、随机种子编辑器生成器。

### 能否在游戏运行时生成？

0.1.1 暂不支持。生成发生在 Unreal Editor 中，生成的 Actor 与实例化 Mesh
组件可以随编辑后的关卡保存。

### 支持哪些 Unreal Engine 版本？

0.1.1 已针对 Unreal Engine 5.8 构建和验证；Unreal Engine 5.5 用户可使用
0.1.0，其他版本尚不在测试兼容矩阵中。

更多答案见[完整中文 FAQ](Docs/faq.zh-CN.md)。

## 参与贡献

欢迎提交 Issue 和 Pull Request。报告生成问题时，请附上 Unreal Engine 版本、
区域类型、随机种子、配方资产路径，以及返回的警告或错误。

- [创建 Issue](https://github.com/SeeleAI/seele-scatter-regions/issues)
- [参与 Discussions](https://github.com/SeeleAI/seele-scatter-regions/discussions)
- [查看源码](https://github.com/SeeleAI/seele-scatter-regions)

## 许可证

Seele Scatter Regions 采用 [MIT License](LICENSE)，归属及声明信息见
[NOTICE.md](NOTICE.md)。
