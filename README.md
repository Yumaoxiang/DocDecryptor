# 上行
cd /d/利尔达/DocDecryptor

# 1. 查看当前状态（强烈建议加上）
git status

# 2. 添加所有修改
git add .

# 3. 提交（写清楚本次修改内容）
git commit -m "update: 更新NT26-H PPT V0.4"

# 4. 推送到GitHub
git push



# 解密时必须在相同文件夹
git fetch origin
git reset --hard origin/main