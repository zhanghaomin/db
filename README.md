[![Build Status](https://travis-ci.com/zhanghaomin/db.svg?branch=master)](https://travis-ci.com/zhanghaomin/db)
# db
simple database 

v0.0.1
1. crud
2. 不考虑索引 
3. 块内紧密存储
4. 储存每个块的剩余空间（初期用数组）
5. 一张表最大4096个page
6. sql -> ast -> execute

TODO:
1. 存储结构增加page_directory
2. update
3. delete

v0.0.2
1. 增加lru，可以设置最大缓存页数
2. 移除单表最大4096个page限制