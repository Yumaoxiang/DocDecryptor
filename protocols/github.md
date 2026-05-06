🚀 Lierda SDK - 业务代码同步

在执行 git push 之前，可以输入以下命令来检查当前文件夹绑定的“云端地址”：
git remote -v

语义化提交： 
* 新功能用 feat: ...
修 Bug 用 fix: ...
改文档用 docs: ...


# 第一步：精准定位 (cd)在 Git Bash 中直接执行（建议复制此行）：
cd "/d/Trae/OpenCPU_SDK_General/SDK/PLAT/project/ec7xx_0h00/ap/apps/lierda_app/customer_app"

# 第二步：标准推送三部曲 (Standard Workflow)
按顺序执行以下三行指令：步骤命令作用状态检查
1. 扫描
git add .
将所有修改装入“待上传篮子”git status 变绿
2. 提交
git commit -m "feat: 描述内容"
给这次修改打包并起个名字产生一个新的 Commit
3. 推送
git push
发送到 GitHub 云端GitHub 网页实时更新

