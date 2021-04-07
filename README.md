# WEngine
WEngine是一个基于DirectX Raytracing API实现的路径追踪渲染器。在RTX硬件光追加持下，渲染具有15万面的场景时，仍能够达到50fps以上的帧率。
渲染器内嵌ImGUI库实现了基本的用户交互界面，支持对模型变换矩阵和材质参数的动态修改。

<img src="./pics/坐标变换1.gif" width = "400" height = "313" alt="修改模型矩阵" align=center />

渲染器能够解析XML格式的场景描述文件，支持对obj格式的模型的加载，支持纹理贴图、切线空间法线贴图。参照pbrt实现玻璃、镜面、黏土、金属、塑料等材质。
- 塑料材质

<img src="./pics/PlasticBunny.PNG" width = "400" height = "313" alt="塑料材质" align=center />

- 金属材质（铁）

<img src="./pics/MetalBunny.PNG" width = "400" height = "313" alt="金属材质" align=center />
