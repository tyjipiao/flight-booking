# 机票预订系统（C++ + MySQL 控制台版）

一个可以直接编译运行的小型机票预订系统（C++17 控制台 + MySQL 8）。

| 依赖 | 要求 |
| --- | --- |
| 编译器 | Visual Studio 2019/2022/2026 的 **生成工具**（含「使用 C++ 的桌面开发」工作负载，提供 `cl.exe` + `vcvars64.bat`） |
| 数据库客户端库 | **MySQL Server 8.0+** 安装目录下的 `include\mysql.h`、`lib\libmysql.lib`、`lib\libmysql.dll`（MySQL Shell / Workbench 不含这些） |
| 数据库服务 | MySQL 8.0+，默认 `127.0.0.1:3306` |

不需要 .NET SDK、不需要 NuGet 拉包、不需要 Visual Studio 图形界面——全部离线可编译。

**路径不用手改**：`build.bat` 会自动探测。找不到时可以用环境变量指定：

```powershell
set VCVARS64=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat
set MYSQL_DIR=C:\Program Files\MySQL\MySQL Server 8.0
```

探测顺序：

- **VS**：`VCVARS64` 环境变量 → 项目下 `tools\vcvars64.bat` → 用 `vswhere.exe` 查已安装的 C++ 工作负载 → 扫描 `%ProgramFiles%` / `%ProgramFiles(x86)%` 下的 Visual Studio 目录
- **MySQL**：`MYSQL_DIR` 环境变量 → 常见路径（`Program Files\MySQL\MySQL Server 8.0/8.4/9.0`、`C:\MySQL\...`、`D:\MySQL\...`）→ 扫描 C/D/E/F 盘的 `Program Files\MySQL`、`MySQL`、`mysql` 目录，以 `include\mysql.h` 是否存在为准

---

## 一、三步跑起来

### 第 1 步：建库（只需一次）

打开 **MySQL Workbench**（你已安装 8.0），用 **root** 账号连接本地实例，然后：

1. `File → Open SQL Script...`，选 `sql\schema.sql`
2. 点闪电图标（Execute All）执行

脚本会做四件事：建 `flight_booking` 库 → 建三张表 → 插入 6 条示例航班 → 创建专用账号 `flight`（只给这个库的增删改查权限，程序不用 root）。脚本里密码写的是占位符 `CHANGE_ME_2026`，**执行前请先改成你自己的密码**。

执行完最后会返回一行统计，能看到航班条数就说明成功了。

### 第 2 步：准备连接配置

仓库里**不含** `config.ini`（里面有数据库密码，已被 `.gitignore` 排除）。请从模板复制一份：

```powershell
cd D:\Projects\FlightBooking
copy config.example.ini config.ini
```

然后编辑 `config.ini`，把 `password` 填成你在上一步 `schema.sql` 里设置的密码（脚本里默认写的是占位符 `CHANGE_ME_2026`）：

```ini
[mysql]
host=127.0.0.1
port=3306
user=flight
password=你的密码
database=flight_booking

[admin]
password=admin123
```

改完不用重新编译；要注意 `build.bat` 会把 `config.ini` 复制到 `bin\`，所以**以后改配置请改项目根目录那份**，再跑一次 `build.bat` 同步。

### 第 3 步：编译并运行

- 双击 **`build.bat`** → 自动找到 `vcvars64.bat` 和 MySQL 客户端库 → 编译 `src\*.cpp` → 把 `libmysql.dll`、`libcrypto-3-x64.dll`、`libssl-3-x64.dll` 和 `config.ini` 复制到 `bin\`
- 双击 **`run.bat`**（或直接运行 `bin\FlightBooking.exe`）

编译成功的标志是两行路径提示加一行 `[OK] ...\bin\FlightBooking.exe`：

```
[1/3] vcvars64 : C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat
[1/3] MySQL    : C:\Program Files\MySQL\MySQL Server 8.0
[2/3] Compiling...
[3/3] Copying runtime DLLs and config...
[OK] ...\bin\FlightBooking.exe
```

探测失败时会明确告诉你缺什么、怎么用环境变量指定，而不是抛一堆 `cl 不是内部或外部命令`。

---

## 二、功能

主菜单：

```
1) 查询航班    —— 按出发城市 / 到达城市 / 日期组合筛选，空回车表示不限
2) 预订机票    —— 选航班号 → 填姓名、身份证、手机 → 事务内扣座并生成订单号
3) 查询订单    —— 输入订单号或身份证号；空回车列出最近 100 条
4) 退票        —— 输入订单号确认后退票，座位自动回补
5) 管理后台    —— 口令 admin123：新增航班 / 查看全部订单 / 运营统计
0) 退出
```

示例数据是贵阳出发的几条航线（贵阳→北京/上海/广州/成都，另有两条返程），日期在 2026 年 11 月，方便直接演示。

---

## 三、目录结构

```
D:\Projects\FlightBooking
├─ src\
│  ├─ main.cpp        菜单、控制台输出（中文对齐按显示宽度计算）、配置读取
│  ├─ db.h            MySQL 连接 RAII 封装：exec / query / escape / 事务
│  ├─ service.h       业务接口与数据结构（Flight / OrderView）
│  └─ service.cpp     查询、订票、退票、新增航班、统计的具体 SQL
├─ sql\schema.sql     建库建表 + 示例数据 + 专用账号
├─ config.example.ini 配置模板（复制成 config.ini 后填密码）
├─ config.ini         本地实际配置（含密码，已 gitignore，不入库）
├─ build.bat          一键编译
├─ run.bat            一键运行
└─ bin\               编译产物（exe + 必需的 DLL + config.ini）
```

---

## 四、数据库设计

| 表 | 作用 | 关键点 |
| --- | --- | --- |
| `flights` | 航班 | `flight_no` 唯一；`seats_left` 余票；`(origin, destination, depart_time)` 联合索引配合查询 |
| `passengers` | 乘客 | `id_card` 唯一，同一人多次订票复用一条记录 |
| `orders` | 订单 | `order_no` 唯一；外键指向航班与乘客；`status` 1 已订 / 0 已退 |

## 五、几个值得说明的实现点

1. **防超卖**：订票走事务，先 `SELECT ... FOR UPDATE` 锁住航班行再判断余票，扣座与写订单同事务提交；余票为 0 直接回滚。退票同样在事务里完成「改订单状态 + 回补座位」。
2. **订单号**：`FB` + `yyMMddHHmmss` + 3 位随机数（17 位）。极小概率撞号时，捕获 1062 唯一键冲突自动重试 5 次。
3. **SQL 注入**：所有用户输入都过 `mysql_real_escape_string` 再拼进语句，程序里没有裸拼接的输入值。
4. **中文输出对齐**：控制台按 UTF-8 输出（`SetConsoleOutputCP(CP_UTF8)`），表格列宽按「中文占 2 列」计算显示宽度，所以中文列也能对齐。
5. **配置与程序分离**：连接信息全在 `config.ini`，依次在 exe 同目录、上一级、当前目录、`..\` 查找，换机器只改配置不重编译。

---

## 六、常见问题

| 现象 | 原因与处理 |
| --- | --- |
| `Access denied for user 'flight'@'localhost'` | 还没执行 `schema.sql`，或 `config.ini` 里的密码和脚本里的不一致 |
| `Unknown database 'flight_booking'` | 同上，库还没建 |
| 双击 exe 一闪而过 / 提示找不到 DLL | 用 `run.bat` 启动，或确认 `bin\` 里有 `libmysql.dll`、`libcrypto-3-x64.dll`、`libssl-3-x64.dll` 三个文件 |
| `[ERROR] Visual Studio C++ build tools not found` | 没装 VS 生成工具的 C++ 工作负载，或装在非常规位置：`set VCVARS64=<vcvars64.bat 全路径>` 后重试 |
| `[ERROR] MySQL client headers not found` | 只装了 MySQL Shell/Workbench（不含 `mysql.h`）：装 MySQL Server 8.0+，或 `set MYSQL_DIR=<服务端目录>` 后重试 |
| 控制台中文变乱码 | 终端字体不支持，换成「新宋体」或在 Windows Terminal 里运行 |

---

## 七、可以继续扩展的方向

- 座位号选择（给 `orders` 加 `seat_no`，航班加已占座位表）
- 乘客登录 / 注册，区分普通用户与管理员
- 改签（退旧订新，同一事务）
- 报表导出 CSV、按航线/月份统计收入
- 换成图形界面：可以用 Win32 对话框或 Qt 包一层，业务层 `service.cpp` 不用改

> 若以后装了完整的 Visual Studio IDE，把 `src` 下的 4 个文件加进一个空的 C++ 控制台项目，并在项目属性里配好「附加包含目录」和「附加库目录」指向 MySQL 的 `include` / `lib`，即可在 IDE 里调试。
