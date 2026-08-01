# 在服务器仓库根目录执行
mkdir -p "5_重构代码v2/6_数值实验"
cp -r /path/to/extracted/ZTT_OR_GITHUB_NUMERICAL_EXPERIMENTS_20260801_1535/5_重构代码v2/6_数值实验/* \
      "5_重构代码v2/6_数值实验/"

git status --short
git add "5_重构代码v2/6_数值实验"
git commit -m "Add formal numerical experiment workspace"
git push origin master
