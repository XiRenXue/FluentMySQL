#include "database.h"
#include <regex>
#include <algorithm>
#include <cctype>
#include <ranges>
auto MySQLResult::GetColumnIndex(std::string_view ColumnName) const -> std::optional<std::size_t>
{
    const auto IteratorPosition = std::ranges::find(ColumnNames, ColumnName);
    if (IteratorPosition != ColumnNames.end())
        return std::distance(ColumnNames.begin(), IteratorPosition);
    return std::nullopt;
}

auto MySQLResult::HasColumn(std::string_view ColumnName) const -> bool
{
    return std::ranges::any_of(ColumnNames, [ColumnName](const std::string& NameValue) { return NameValue == ColumnName; });
}

TransactionGuard::TransactionGuard(sql::Connection* ConnectionPtr) : Connection(ConnectionPtr)
{
    if (Connection)
    {
        try { Connection->setAutoCommit(false); }
        catch (...) { }
    }
}

TransactionGuard::~TransactionGuard()
{
    if (Connection && !IsCommitted && !IsRolledBack)
    {
        try
        {
            Connection->rollback();
            Connection->setAutoCommit(true);
        }
        catch (...) { }
    }
}

auto TransactionGuard::Commit() -> void
{
    if (Connection && !IsCommitted && !IsRolledBack)
    {
        Connection->commit();
        Connection->setAutoCommit(true);
        IsCommitted = true;
    }
}

auto TransactionGuard::Rollback() -> void
{
    if (Connection && !IsCommitted && !IsRolledBack)
    {
        Connection->rollback();
        Connection->setAutoCommit(true);
        IsRolledBack = true;
    }
}

auto SQLSanitizer::DetectSQLInjection(std::string_view SqlQuery) -> bool
{
    constexpr std::array<std::string_view, 6> DangerousPatterns = 
    {
        R"((\bOR\b|\bAND\b)\s+['\"]?\d+['\"]?\s*=\s*['\"]?\d+)",
        R"(;\s*(DROP|DELETE|UPDATE|INSERT)\s+)",
        R"(--|\#|/\*)",
        R"(\bUNION\b.*\bSELECT\b)",
        R"(\bEXEC\b|\bEXECUTE\b)",
        R"(\bxp_cmdshell\b)",
    };
    std::string UpperCaseQuery;
    UpperCaseQuery.reserve(SqlQuery.size());
    std::ranges::transform(SqlQuery, std::back_inserter(UpperCaseQuery), [](unsigned char CharValue) { return std::toupper(CharValue); });
    for (const auto& PatternString : DangerousPatterns)
    {
        try
        {
            const std::regex PatternRegex(PatternString.data(), std::regex::icase);
            if (std::regex_search(UpperCaseQuery, PatternRegex))
                return true;
        }
        catch (...)
        {
            continue;
        }
    }
    return false;
}

auto SQLSanitizer::IsValidIdentifier(std::string_view IdentifierName) -> bool
{
    if (IdentifierName.empty()) [[unlikely]]
        return false;
    if (std::isdigit(static_cast<unsigned char>(IdentifierName[0]))) [[unlikely]]
        return false;
    return std::ranges::all_of(IdentifierName, [](unsigned char CharValue) { return std::isalnum(CharValue) || CharValue == '_'; });
}

auto SQLSanitizer::EscapeString(std::string_view InputString) -> std::string
{
    std::string EscapedString;
    EscapedString.reserve(InputString.length() * 2);
    for (const char CharValue : InputString)
    {
        switch (CharValue)
        {
        case '\'': EscapedString += "\\'"; break;
        case '\"': EscapedString += "\\\""; break;
        case '\\': EscapedString += "\\\\"; break;
        case '\0': EscapedString += "\\0"; break;
        case '\n': EscapedString += "\\n"; break;
        case '\r': EscapedString += "\\r"; break;
        case '\t': EscapedString += "\\t"; break;
        default: EscapedString += CharValue; break;
        }
    }
    return EscapedString;
}

auto SQLSanitizer::BuildParameterizedQuery(std::string_view QueryTemplate, const std::vector<std::string>& ParameterList) -> std::string
{
    std::string ResultQuery{ QueryTemplate };
    std::size_t ParameterIndex = 0;
    std::size_t SearchPosition = 0;
    while ((SearchPosition = ResultQuery.find('?', SearchPosition)) != std::string::npos)
    {
        if (ParameterIndex >= ParameterList.size()) [[unlikely]]
            break;
        const std::string EscapedParameter = std::format("'{}'", EscapeString(ParameterList[ParameterIndex]));
        ResultQuery.replace(SearchPosition, 1, EscapedParameter);
        SearchPosition += EscapedParameter.length();
        ++ParameterIndex;
    }
    return ResultQuery;
}

MySQLWrapper::MySQLWrapper()
{
    try
    {
        DriverInstance = sql::mysql::get_driver_instance();
    }
    catch (const sql::SQLException& Exception)
    {
        LastErrorMessage = std::format("驱动初始化错误: {} (代码: {})", Exception.what(), Exception.getErrorCode());
    }
}

MySQLWrapper::~MySQLWrapper()
{
    Disconnect();
}

auto MySQLWrapper::Connect(const MySQLConfig& ConfigParam) -> bool
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    return ConnectInternal(ConfigParam);
}

auto MySQLWrapper::ConnectInternal(const MySQLConfig& ConfigParam) -> bool
{
    try
    {
        DisconnectInternal();
        CurrentConfig = ConfigParam;
        if (!DriverInstance) [[unlikely]]
        {
            LastErrorMessage = "驱动未初始化";
            LogError(LastErrorMessage);
            return false;
        }
        const std::string ConnectionUrl = std::format("tcp://{}:{}", ConfigParam.Host, ConfigParam.Port);
        ActiveConnection.reset(DriverInstance->connect(ConnectionUrl, ConfigParam.User, ConfigParam.Password));
        if (!ActiveConnection) [[unlikely]]
        {
            LastErrorMessage = "创建连接失败";
            LogError(LastErrorMessage);
            return false;
        }
        ActiveConnection->setClientOption("OPT_CONNECT_TIMEOUT", &ConfigParam.ConnectTimeout);
        ActiveConnection->setClientOption("OPT_READ_TIMEOUT", &ConfigParam.ReadTimeout);
        ActiveConnection->setClientOption("OPT_WRITE_TIMEOUT", &ConfigParam.WriteTimeout);
        if (ConfigParam.EnableAutoReconnect)
        {
            bool AutoReconnectFlag = true;
            ActiveConnection->setClientOption("OPT_RECONNECT", &AutoReconnectFlag);
        }
        if (!ConfigParam.Database.empty())
            ActiveConnection->setSchema(ConfigParam.Database);
        const std::unique_ptr<sql::Statement> Statement(ActiveConnection->createStatement());
        Statement->execute(std::format("SET NAMES {}", ConfigParam.Charset));
        IsConnected = true;
        LastSuccessfulConfig = ConfigParam;
        LastErrorMessage.clear();
        Log(std::format("已连接到 {}:{}", ConfigParam.Host, ConfigParam.Port));
        return true;
    }
    catch (const sql::SQLException& Exception)
    {
        LastErrorMessage = std::format("连接错误: {} (代码: {}, 状态: {})", Exception.what(), Exception.getErrorCode(), Exception.getSQLState());
        LogError(LastErrorMessage);
        IsConnected = false;
        return false;
    }
}

auto MySQLWrapper::DisconnectInternal() noexcept -> void
{
    try
    {
        if (ActiveConnection)
        {
            ActiveConnection->close();
            ActiveConnection.reset();
        }
        IsConnected = false;
        Log("已断开数据库连接");
    }
    catch (const sql::SQLException& Exception)
    {
        LastErrorMessage = std::format("断开连接错误: {}", Exception.what());
        LogError(LastErrorMessage);
    }
    catch (...) { }
}

auto MySQLWrapper::Disconnect() noexcept -> void
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    DisconnectInternal();
}

auto MySQLWrapper::ReconnectInternal() -> bool
{
    DisconnectInternal();
    if (LastSuccessfulConfig.Host.empty()) [[unlikely]]
    {
        LastErrorMessage = "没有可用的先前连接配置";
        return false;
    }
    return ConnectInternal(LastSuccessfulConfig);
}

auto MySQLWrapper::Reconnect() -> bool
{
    Log("尝试重新连接...");
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    return ReconnectInternal();
}

auto MySQLWrapper::Ping() -> bool
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    return PingInternal();
}

auto MySQLWrapper::PingInternal() -> bool
{
    if (!ActiveConnection) [[unlikely]]
        return false;
    try
    {
        const std::unique_ptr<sql::Statement> Statement(ActiveConnection->createStatement());
        if (Statement->execute("SELECT 1"))
        {
            std::unique_ptr<sql::ResultSet> ResultSet(Statement->getResultSet());
            while (ResultSet && ResultSet->next()) { /* 消费结果集 */ }
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

auto MySQLWrapper::Execute(const std::string& SqlCommand) -> MySQLResult
{
    return ExecuteInternal(SqlCommand, false);
}

auto MySQLWrapper::Query(const std::string& SqlQuery) -> MySQLResult
{
    return ExecuteInternal(SqlQuery, true);
}

auto MySQLWrapper::ExecuteParameterized(std::string_view QueryTemplate, const std::vector<std::string>& ParameterList) -> MySQLResult
{
    if (SQLSanitizer::DetectSQLInjection(QueryTemplate)) [[unlikely]]
    {
        MySQLResult ErrorResult;
        ErrorResult.ErrorMessage = "检测到潜在的 SQL 注入";
        ErrorResult.Success = false;
        LogError(ErrorResult.ErrorMessage);
        return ErrorResult;
    }
    const std::string FinalQuery = SQLSanitizer::BuildParameterizedQuery(QueryTemplate, ParameterList);
    return Query(FinalQuery);
}

auto MySQLWrapper::ExecuteBatch(const std::vector<std::string>& SqlStatements) -> std::vector<MySQLResult>
{
    std::vector<MySQLResult> ResultList;
    ResultList.reserve(SqlStatements.size());
    for (const auto& SqlStatement : SqlStatements)
        ResultList.push_back(Execute(SqlStatement));
    return ResultList;
}

auto MySQLWrapper::BeginTransaction() -> bool
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    if (!ActiveConnection) [[unlikely]]
        return false;
    try
    {
        ActiveConnection->setAutoCommit(false);
        Log("事务已开始");
        return true;
    }
    catch (const sql::SQLException& Exception)
    {
        LastErrorMessage = std::format("开始事务错误: {}", Exception.what());
        LogError(LastErrorMessage);
        return false;
    }
}

auto MySQLWrapper::CommitTransaction() -> bool
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    if (!ActiveConnection) [[unlikely]]
        return false;
    try
    {
        ActiveConnection->commit();
        ActiveConnection->setAutoCommit(true);
        Log("事务已提交");
        return true;
    }
    catch (const sql::SQLException& Exception)
    {
        LastErrorMessage = std::format("提交事务错误: {}", Exception.what());
        LogError(LastErrorMessage);
        return false;
    }
}

auto MySQLWrapper::RollbackTransaction() -> bool
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    if (!ActiveConnection) [[unlikely]]
        return false;
    try
    {
        ActiveConnection->rollback();
        ActiveConnection->setAutoCommit(true);
        Log("事务已回滚");
        return true;
    }
    catch (const sql::SQLException& Exception)
    {
        LastErrorMessage = std::format("回滚事务错误: {}", Exception.what());
        LogError(LastErrorMessage);
        return false;
    }
}

auto MySQLWrapper::GetTransactionGuard() -> std::unique_ptr<TransactionGuard>
{
    if (!ActiveConnection) [[unlikely]]
        return nullptr;
    return std::make_unique<TransactionGuard>(ActiveConnection.get());
}

auto MySQLWrapper::PrepareStatement(std::string_view SqlQuery) -> std::expected<std::unique_ptr<sql::PreparedStatement>, std::string>
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    if (!ActiveConnection) [[unlikely]]
        return std::unexpected("未连接到数据库");
    try
    {
        return std::unique_ptr<sql::PreparedStatement>(
            ActiveConnection->prepareStatement(std::string{ SqlQuery }));
    }
    catch (const sql::SQLException& Exception)
    {
        return std::unexpected(std::format("预处理语句错误: {}", Exception.what()));
    }
}

auto MySQLWrapper::ExecutePrepared(sql::PreparedStatement* StatementPtr, const std::vector<std::string>& ParameterList) -> MySQLResult
{
    MySQLResult ResultData;
    if (!StatementPtr) [[unlikely]]
    {
        ResultData.ErrorMessage = "空语句指针";
        return ResultData;
    }
    try
    {
        for (std::size_t Index = 0; Index < ParameterList.size(); ++Index)
            StatementPtr->setString(static_cast<unsigned int>(Index + 1), ParameterList[Index]);
        if (StatementPtr->execute())
        {
            const std::unique_ptr<sql::ResultSet> ResultSet(StatementPtr->getResultSet());
            sql::ResultSetMetaData* MetaData = ResultSet->getMetaData();
            const int ColumnCount = MetaData->getColumnCount();
            for (int Index = 1; Index <= ColumnCount; ++Index)
                ResultData.ColumnNames.push_back(MetaData->getColumnName(Index));
            while (ResultSet->next())
            {
                MySQLRow RowData;
                for (int Index = 1; Index <= ColumnCount; ++Index)
                {
                    if (ResultSet->isNull(Index))
                        RowData.Fields.push_back("NULL");
                    else
                        RowData.Fields.push_back(ResultSet->getString(Index));
                }
                ResultData.Rows.push_back(std::move(RowData));
            }
        }
        else
            ResultData.AffectedRows = StatementPtr->getUpdateCount();
        ResultData.Success = true;
        UpdateStatistics(true);
    }
    catch (const sql::SQLException& Exception)
    {
        ResultData.ErrorMessage = std::format("执行预处理语句错误: {}", Exception.what());
        UpdateStatistics(false);
        LogError(ResultData.ErrorMessage);
    }
    return ResultData;
}

auto MySQLWrapper::GetDatabases() -> std::expected<std::vector<std::string>, std::string>
{
    const MySQLResult ResultData = Query("SHOW DATABASES");
    if (!ResultData.Success) [[unlikely]]
        return std::unexpected(ResultData.ErrorMessage);
    std::vector<std::string> DatabaseList;
    DatabaseList.reserve(ResultData.Rows.size());
    for (const auto& RowData : ResultData.Rows)
    {
        if (!RowData.Fields.empty())
            DatabaseList.push_back(RowData.Fields[0]);
    }
    return DatabaseList;
}

auto MySQLWrapper::GetTables(std::string_view DatabaseName) -> std::expected<std::vector<std::string>, std::string>
{
    std::string SqlQuery = "SHOW TABLES";
    if (!DatabaseName.empty())
        SqlQuery += std::format(" FROM `{}`", DatabaseName);
    const MySQLResult ResultData = Query(SqlQuery);
    if (!ResultData.Success) [[unlikely]]
        return std::unexpected(ResultData.ErrorMessage);
    std::vector<std::string> TableList;
    TableList.reserve(ResultData.Rows.size());
    for (const auto& RowData : ResultData.Rows)
    {
        if (!RowData.Fields.empty())
            TableList.push_back(RowData.Fields[0]);
    }
    return TableList;
}

auto MySQLWrapper::GetTableStructure(std::string_view TableName) -> std::expected<MySQLResult, std::string>
{
    if (!SQLSanitizer::IsValidIdentifier(TableName)) [[unlikely]]
        return std::unexpected("无效的表名");
    const MySQLResult ResultData = Query(std::format("DESCRIBE `{}`", TableName));
    if (!ResultData.Success) [[unlikely]]
        return std::unexpected(ResultData.ErrorMessage);
    return ResultData;
}

auto MySQLWrapper::GetTableIndexes(std::string_view TableName) -> std::expected<MySQLResult, std::string>
{
    if (!SQLSanitizer::IsValidIdentifier(TableName)) [[unlikely]]
        return std::unexpected("无效的表名");
    const MySQLResult ResultData = Query(std::format("SHOW INDEX FROM `{}`", TableName));
    if (!ResultData.Success) [[unlikely]]
        return std::unexpected(ResultData.ErrorMessage);
    return ResultData;
}

auto MySQLWrapper::GetServerVersion() -> std::expected<std::string, std::string>
{
    const MySQLResult ResultData = Query("SELECT VERSION()");
    if (!ResultData.Success || ResultData.Rows.empty() || ResultData.Rows[0].Fields.empty()) [[unlikely]]
        return std::unexpected("获取服务器版本失败");
    return ResultData.Rows[0].Fields[0];
}

auto MySQLWrapper::GetCurrentDatabase() -> std::expected<std::string, std::string>
{
    const MySQLResult ResultData = Query("SELECT DATABASE()");
    if (!ResultData.Success || ResultData.Rows.empty() || ResultData.Rows[0].Fields.empty()) [[unlikely]]
        return std::unexpected("获取当前数据库失败");
    return ResultData.Rows[0].Fields[0];
}

auto MySQLWrapper::EscapeString(const std::string& InputString) const -> std::string
{
    return SQLSanitizer::EscapeString(InputString);
}

auto MySQLWrapper::ValidateSQL(std::string_view SqlQuery) const -> bool
{
    return !SQLSanitizer::DetectSQLInjection(SqlQuery);
}

auto MySQLWrapper::SetQueryTimeout(unsigned int TimeoutSeconds) -> void
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    if (ActiveConnection)
    {
        try
        {
            ActiveConnection->setClientOption("OPT_READ_TIMEOUT", &TimeoutSeconds);
        }
        catch (...) { }
    }
}

auto MySQLWrapper::SetResultLimit(std::size_t MaxRows) -> void
{
    MaxResultRows = MaxRows;
}

auto MySQLWrapper::GetLastError() const -> std::string
{
    return LastErrorMessage;
}

auto MySQLWrapper::SetLogCallback(std::function<void(std::string_view)> CallbackFunc) -> void
{
    LogCallback = std::move(CallbackFunc);
}

auto MySQLWrapper::Log(std::string_view Message) const -> void
{
    if (LogCallback)
        LogCallback(Message);
}

auto MySQLWrapper::LogError(std::string_view ErrorMessage) -> void
{
    LastErrorMessage = ErrorMessage;
    Log(std::format("错误: {}", ErrorMessage));
}

auto MySQLWrapper::ConnectExpected(const MySQLConfig& ConfigParam) -> std::expected<void, std::string>
{
    const bool IsSuccess = Connect(ConfigParam);
    if (IsSuccess)
        return {};
    else
        return std::unexpected(LastErrorMessage);
}

auto MySQLWrapper::QueryExpected(const std::string& SqlQuery) -> std::expected<MySQLResult, std::string>
{
    MySQLResult ResultData = Query(SqlQuery);
    if (ResultData.Success)
        return ResultData;
    else
        return std::unexpected(ResultData.ErrorMessage);
}

auto MySQLWrapper::GetStatistics() const -> std::tuple<uint64_t, uint64_t, uint64_t>
{
    return 
    {
        Statistics.TotalQueries.load(),
        Statistics.SuccessfulQueries.load(),
        Statistics.FailedQueries.load()
    };
}

auto MySQLWrapper::ResetStatistics() -> void
{
    Statistics.TotalQueries = 0;
    Statistics.SuccessfulQueries = 0;
    Statistics.FailedQueries = 0;
}

auto MySQLWrapper::GetConnectionInfo() const -> std::string
{
    if (!IsConnected)
        return "未连接";
    return std::format("{}@{}:{}/{}", CurrentConfig.User, CurrentConfig.Host, CurrentConfig.Port, CurrentConfig.Database);
}

auto MySQLWrapper::UpdateStatistics(bool IsSuccess) noexcept -> void
{
    Statistics.TotalQueries++;
    if (IsSuccess)
        Statistics.SuccessfulQueries++;
    else
        Statistics.FailedQueries++;
    Statistics.LastQueryTime = std::chrono::steady_clock::now();
}

auto MySQLWrapper::ExecuteInternal(const std::string& SqlQuery, bool IsQuery) -> MySQLResult
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    MySQLResult ResultData;
    const auto StartTime = std::chrono::steady_clock::now();
    if (!ValidateConnectionInternal()) [[unlikely]]
    {
        ResultData.ErrorMessage = "连接验证失败";
        UpdateStatistics(false);
        return ResultData;
    }
    try
    {
        const std::unique_ptr<sql::Statement> Statement(ActiveConnection->createStatement());
        if (IsQuery)
        {
            const std::unique_ptr<sql::ResultSet> ResultSet(Statement->executeQuery(SqlQuery));
            sql::ResultSetMetaData* MetaData = ResultSet->getMetaData();
            const int ColumnCount = MetaData->getColumnCount();
            for (int Index = 1; Index <= ColumnCount; ++Index)
                ResultData.ColumnNames.push_back(MetaData->getColumnName(Index));
            std::size_t RowCount = 0;
            while (ResultSet->next())
            {
                if (MaxResultRows > 0 && RowCount >= MaxResultRows)
                    break;
                MySQLRow RowData;
                for (int Index = 1; Index <= ColumnCount; ++Index)
                {
                    if (ResultSet->isNull(Index))
                        RowData.Fields.push_back("NULL");
                    else
                        RowData.Fields.push_back(ResultSet->getString(Index));
                }
                ResultData.Rows.push_back(std::move(RowData));
                ++RowCount;
            }
            ResultData.AffectedRows = ResultData.Rows.size();
        }
        else
        {
            const bool HasResult = Statement->execute(SqlQuery);
            if (!HasResult)
                ResultData.AffectedRows = Statement->getUpdateCount();
        }
        ResultData.Success = true;
        UpdateStatistics(true);
        LastErrorMessage.clear();
    }
    catch (const sql::SQLException& Exception)
    {
        ResultData.ErrorMessage = std::format("{} 错误: {} (代码: {}, 状态: {})", IsQuery ? "查询" : "执行", Exception.what(), Exception.getErrorCode(), Exception.getSQLState());
        UpdateStatistics(false);
        LogError(ResultData.ErrorMessage);
    }
    const auto EndTime = std::chrono::steady_clock::now();
    ResultData.ExecutionTime = std::chrono::duration_cast<std::chrono::milliseconds>(EndTime - StartTime);
    return ResultData;
}

auto MySQLWrapper::ValidateConnectionInternal() -> bool
{
    if (!ActiveConnection || ActiveConnection->isClosed()) [[unlikely]]
    {
        LogError("连接丢失，尝试重新连接...");
        return ReconnectInternal();
    }
    try
    {
        std::unique_ptr<sql::Statement> Statement(ActiveConnection->createStatement());
        if (Statement->execute("SELECT 1"))
        {
            std::unique_ptr<sql::ResultSet> ResultSet(Statement->getResultSet());
            while (ResultSet && ResultSet->next()) { /* 消费结果集 */ }
        }
        return true;
    }
    catch (const sql::SQLException&)
    {
        return ReconnectInternal();
    }
}

ConnectionPool::ConnectionPool(const MySQLConfig& ConfigParam, std::size_t MaxSize) : Configuration(ConfigParam), MaxPoolSize(MaxSize)
{
    Pool.reserve(MaxPoolSize);
}

ConnectionPool::~ConnectionPool()
{
    std::lock_guard<std::mutex> Lock(PoolMutex);
    for (auto& PooledConn : Pool)
    {
        try
        {
            if (PooledConn.Connection && !PooledConn.Connection->isClosed())
                PooledConn.Connection->close();
        }
        catch (...) { }
    }
    Pool.clear();
}

auto ConnectionPool::AcquireConnection() -> std::unique_ptr<sql::Connection>
{
    std::lock_guard<std::mutex> Lock(PoolMutex);
    for (auto& PooledConn : Pool)
    {
        if (!PooledConn.IsInUse && PooledConn.Connection)
        {
            try
            {
                if (!PooledConn.Connection->isClosed())
                {
                    std::unique_ptr<sql::Statement> TestStmt(PooledConn.Connection->createStatement());
                    TestStmt->execute("SELECT 1");
                    PooledConn.IsInUse = true;
                    PooledConn.LastUsedTime = std::chrono::steady_clock::now();
                    return std::move(PooledConn.Connection);
                }
            }
            catch (...)
            {
                PooledConn.Connection.reset();
            }
        }
    }
    if (Pool.size() < MaxPoolSize)
    {
        try
        {
            sql::Driver* Driver = sql::mysql::get_driver_instance();
            const std::string ConnectionUrl = std::format("tcp://{}:{}", Configuration.Host, Configuration.Port);
            std::unique_ptr<sql::Connection> NewConnection(Driver->connect(ConnectionUrl, Configuration.User, Configuration.Password));
            if (NewConnection)
            {
                NewConnection->setClientOption("OPT_CONNECT_TIMEOUT", &Configuration.ConnectTimeout);
                NewConnection->setClientOption("OPT_READ_TIMEOUT", &Configuration.ReadTimeout);
                NewConnection->setClientOption("OPT_WRITE_TIMEOUT", &Configuration.WriteTimeout);
                if (Configuration.EnableAutoReconnect)
                {
                    bool AutoReconnect = true;
                    NewConnection->setClientOption("OPT_RECONNECT", &AutoReconnect);
                }
                if (!Configuration.Database.empty())
                {
                    NewConnection->setSchema(Configuration.Database);
                }
                std::unique_ptr<sql::Statement> Statement(NewConnection->createStatement());
                Statement->execute(std::format("SET NAMES {}", Configuration.Charset));
                PooledConnection NewPooledConn;
                NewPooledConn.Connection = nullptr;
                NewPooledConn.IsInUse = true;
                NewPooledConn.LastUsedTime = std::chrono::steady_clock::now();
                Pool.push_back(std::move(NewPooledConn));
                return NewConnection;
            }
        }
        catch (const sql::SQLException&)
        {
            return nullptr;
        }
    }
    return nullptr;
}

auto ConnectionPool::ReleaseConnection(std::unique_ptr<sql::Connection> ConnectionPtr) -> void
{
    if (!ConnectionPtr)
        return;
    std::lock_guard<std::mutex> Lock(PoolMutex);
    bool isValid = false;
    try
    {
        isValid = !ConnectionPtr->isClosed();
        if (isValid)
        {
            std::unique_ptr<sql::Statement> TestStmt(ConnectionPtr->createStatement());
            TestStmt->execute("SELECT 1");
        }
    }
    catch (...)
    {
        isValid = false;
    }
    if (!isValid)
    {
        try
        {
            if (ConnectionPtr && !ConnectionPtr->isClosed())
                ConnectionPtr->close();
        }
        catch (...) { }
        return;
    }
    for (auto& PooledConn : Pool)
    {
        if (!PooledConn.Connection)
        {
            PooledConn.Connection = std::move(ConnectionPtr);
            PooledConn.IsInUse = false;
            PooledConn.LastUsedTime = std::chrono::steady_clock::now();
            return;
        }
    }
    if (Pool.size() < MaxPoolSize)
    {
        PooledConnection NewPooledConn;
        NewPooledConn.Connection = std::move(ConnectionPtr);
        NewPooledConn.IsInUse = false;
        NewPooledConn.LastUsedTime = std::chrono::steady_clock::now();
        Pool.push_back(std::move(NewPooledConn));
        return;
    }
    try
    {
        if (ConnectionPtr && !ConnectionPtr->isClosed())
            ConnectionPtr->close();
    }
    catch (...) { }
}

auto ConnectionPool::CleanIdleConnections() -> void
{
    std::lock_guard<std::mutex> Lock(PoolMutex);
    const auto CurrentTime = std::chrono::steady_clock::now();
    for (auto& PooledConn : Pool)
    {
        if (!PooledConn.IsInUse && PooledConn.Connection)
        {
            const auto IdleDuration = std::chrono::duration_cast<std::chrono::minutes>(CurrentTime - PooledConn.LastUsedTime);
            if (IdleDuration >= IdleTimeout)
            {
                try
                {
                    if (!PooledConn.Connection->isClosed())
                    {
                        PooledConn.Connection->close();
                    }
                }
                catch (...) { }
                PooledConn.Connection.reset();
            }
        }
    }
    Pool.erase( std::remove_if(Pool.begin(), Pool.end(), [](const PooledConnection& Conn) { return !Conn.Connection; }), Pool.end());
}