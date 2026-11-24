# Confluence 按目录导出

`export-confluence-tree.ps1` 使用官方 REST API v2，默认站点为
`https://haooding.atlassian.net`，空间为 `VTG`。Windows PowerShell 5.1 / PowerShell 7 可运行，无需安装 Python 或其他包。

## 使用

1. 在 <https://id.atlassian.com/manage-profile/security/api-tokens> 创建 **不带 scopes 的普通 API token**（本脚本直接访问站点域名；带 scopes 的 token 使用另一种 API gateway 地址）。设置适合本次导出的过期时间。
2. 打开 PowerShell，执行：

```powershell
cd 'G:\Epic\Unreal projects\VertigoGit'
powershell -NoProfile -ExecutionPolicy Bypass -File '.\Tools\export-confluence-tree.ps1' -OutDir 'G:\VTG-Docs'
```

3. 输入 Atlassian 账号邮箱和 API token。Token 隐藏输入，不写进脚本、日志或导出文件；不要在聊天里发送 token。
4. 完成后打开输出路径下最新导出目录中的 `index.html`。

也可以提前设置 `CONFLUENCE_EMAIL`、`CONFLUENCE_TOKEN` 环境变量，脚本会优先读取。已有旧脚本使用的同名环境变量也适用。不要把 token 明文写进命令历史。

只下载正文，不下载附件：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File '.\Tools\export-confluence-tree.ps1' -OutDir 'G:\VTG-Docs' -SkipAttachments
```

## 导出结果

```text
VTG-20261114-120000-a1b2c3/
  index.html                    # 按 API position 排序的嵌套目录
  manifest.json                 # 页面 ID、父级、路径及附件清单
  failures.json                 # 成功时为 []，失败时记录具体项目
  Audio [123]/
    index.html                  # Audio 页面的正文
    page.json                   # API 返回的页面正文与元数据
    SFX Resource List [456]/
      index.html
      page.json
      Blip Source [789]/
        index.html
        page.json
        attachments/
```

- 每个页面对应一个文件夹，保留可读取的页面父子层级，解析页面上方的 Confluence folder 节点。ID 后缀避免同名覆盖。目录索引按 API 返回的 `position` 排序（缺少时按标题）；资源管理器的排序可能不同。
- 导出当前账号可读取的当前已发布页面及当前附件，非仅本人创建的页面。管理员身份不自动绕过受限页面。不是整个 Confluence 的完整备份：不包含历史版本、草稿、评论、博客、白板、数据库或独立空文件夹。
- 保存渲染 HTML 和原始 API JSON；HTML 有基本表格、图片样式，不是 Confluence 的完全镜像。常见页面链接、已下载附件链接会改成本地相对链接。动态宏、外部嵌入、认证图片及某些 CDN/缩略图地址仍可能需要联网。附件均另列在页面底部，即使正文里的图片不能离线显示，也可打开原始附件。
- 分页自动读取；429 和部分 5xx 状态自动重试。单页或附件失败会继续下载其他项目，结束时返回失败状态并保留 `failures.json`。网络中断可能需要重新运行；每次创建新目录，不是断点续传。
- 默认下载目录在脚本旁，推荐指定项目外的短路径，例如 `G:\VTG-Docs`，避免把文档附件加进游戏仓库。深层目录若遇 Windows 路径过长错误，可缩短输出路径并启用 Windows 长路径支持。
- 脚本只发 GET 请求。附件重定向到其他 HTTPS 域名时不会转发 Authorization。

## 常见错误

- `401`：检查邮箱、token 是否过期，以及是否创建了不带 scopes 的 token。
- `403`：检查账号内容访问权限及组织策略。
- `Space ... not found`：确认空间 key（链接中为 `VTG`），以及账号是否能访问该空间。
- 运行结束提示 partial export：查看输出目录下的 `failures.json`，不要把这次结果当作完整导出。

官方参考：

- <https://developer.atlassian.com/cloud/confluence/basic-auth-for-rest-apis/>
- <https://developer.atlassian.com/cloud/confluence/rest/v2/api-group-page/>
- <https://developer.atlassian.com/cloud/confluence/rest/v2/api-group-folder/>
- <https://developer.atlassian.com/cloud/confluence/rest/v2/api-group-attachment/>
