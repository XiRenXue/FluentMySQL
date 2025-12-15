#pragma once
#include "def.h"
#include <mysql/jdbc.h>
#include <vector>
#include <memory>
#include <expected>
#include <optional>
#include <chrono>
#include <functional>
#include <mutex>
#include <string_view>
#include <atomic>


struct MySQLConfig
{
    std::string Host = "localhost";
    std::string User = "root";
    std::string Password;
    std::string Database;
    unsigned int Port = 3306;
    unsigned int ConnectTimeout = 10;
    unsigned int ReadTimeout = 30;
    unsigned int WriteTimeout = 30;
    bool UseSSL = false;
    std::string SSLCertificate;
    std::string SSLKey;
    std::string SSLCACertificate;
    bool EnableAutoReconnect = true;
    unsigned int MaxRetries = 3;
    std::string Charset = "utf8mb4";
};

struct MySQLRow
{
    std::vector<std::string> Fields;
    [[nodiscard]] auto operator[](std::size_t Index) const -> const std::string&
    {
        return Fields.at(Index);
    }
    [[nodiscard]] constexpr auto Size() const noexcept -> std::size_t
    {
        return Fields.size();
    }
    template<typename T>
    [[nodiscard]] auto GetValue(std::size_t Index) const -> std::optional<T>;
    [[nodiscard]] auto IsNull(std::size_t Index) const noexcept -> bool
    {
        return Index < Fields.size() && Fields[Index] == "NULL";
    }
};

struct MySQLResult
{
    std::vector<std::string> ColumnNames;
    std::vector<MySQLRow> Rows;
    unsigned long long AffectedRows = 0;
    bool Success = false;
    std::string ErrorMessage;
    std::chrono::milliseconds ExecutionTime{ 0 };
    [[nodiscard]] constexpr auto IsEmpty() const noexcept -> bool
    {
        return Rows.empty();
    }
    [[nodiscard]] constexpr auto GetRowCount() const noexcept -> std::size_t
    {
        return Rows.size();
    }
    [[nodiscard]] constexpr auto GetColumnCount() const noexcept -> std::size_t
    {
        return ColumnNames.size();
    }
    [[nodiscard]] auto GetColumnIndex(std::string_view ColumnName) const -> std::optional<std::size_t>;
    [[nodiscard]] auto HasColumn(std::string_view ColumnName) const -> bool;
};

class TransactionGuard
{
private:
    sql::Connection* Connection;
    bool IsCommitted = false;
    bool IsRolledBack = false;
public:
    explicit TransactionGuard(sql::Connection* ConnectionPtr);
    ~TransactionGuard();
    TransactionGuard(const TransactionGuard&) = delete;
    auto operator=(const TransactionGuard&) -> TransactionGuard & = delete;
    TransactionGuard(TransactionGuard&&) = delete;
    auto operator=(TransactionGuard&&) -> TransactionGuard & = delete;
    auto Commit() -> void;
    auto Rollback() -> void;
    [[nodiscard]] constexpr auto GetCommitStatus() const noexcept -> bool { return IsCommitted; }
    [[nodiscard]] constexpr auto GetRollbackStatus() const noexcept -> bool { return IsRolledBack; }
};

class SQLSanitizer
{
public:
    [[nodiscard]] static auto DetectSQLInjection(std::string_view SqlQuery) -> bool;
    [[nodiscard]] static auto IsValidIdentifier(std::string_view IdentifierName) -> bool;
    [[nodiscard]] static auto EscapeString(std::string_view InputString) -> std::string;
    [[nodiscard]] static auto BuildParameterizedQuery(std::string_view QueryTemplate, const std::vector<std::string>& ParameterList) -> std::string;
};

class ConnectionPool
{
private:
    struct PooledConnection
    {
        std::unique_ptr<sql::Connection> Connection;
        std::chrono::steady_clock::time_point LastUsedTime;
        bool IsInUse = false;
    };
    std::vector<PooledConnection> Pool;
    std::mutex PoolMutex;
    MySQLConfig Configuration;
    std::size_t MaxPoolSize = 10;
    std::chrono::minutes IdleTimeout{ 5 };
public:
    explicit ConnectionPool(const MySQLConfig& ConfigParam, std::size_t MaxSize = 10);
    ~ConnectionPool();
    [[nodiscard]] auto AcquireConnection() -> std::unique_ptr<sql::Connection>;
    auto ReleaseConnection(std::unique_ptr<sql::Connection> ConnectionPtr) -> void;
    auto CleanIdleConnections() -> void;
};

class MySQLWrapper
{
private:
    sql::Driver* DriverInstance = nullptr;
    std::unique_ptr<sql::Connection> ActiveConnection;
    bool IsConnected = false;
    MySQLConfig CurrentConfig;
    MySQLConfig LastSuccessfulConfig;
    std::string LastErrorMessage;
    std::size_t MaxResultRows = 0;
    mutable std::mutex ConnectionMutex;
    struct QueryStatistics
    {
        std::atomic<uint64_t> TotalQueries{ 0 };
        std::atomic<uint64_t> SuccessfulQueries{ 0 };
        std::atomic<uint64_t> FailedQueries{ 0 };
        std::chrono::steady_clock::time_point LastQueryTime;
    } Statistics;
    std::function<void(std::string_view)> LogCallback;
    auto DisconnectInternal() noexcept -> void;
    auto ReconnectInternal() -> bool;
    auto ConnectInternal(const MySQLConfig& ConfigParam) -> bool;
    auto PingInternal() -> bool;
    auto ValidateConnectionInternal() -> bool;
    auto ExecuteInternal(const std::string& SqlQuery, bool IsQuery) -> MySQLResult;
public:
    MySQLWrapper();
    ~MySQLWrapper();
    MySQLWrapper(const MySQLWrapper&) = delete;
    auto operator=(const MySQLWrapper&) -> MySQLWrapper & = delete;
    MySQLWrapper(MySQLWrapper&&) noexcept = default;
    auto operator=(MySQLWrapper&&) noexcept -> MySQLWrapper & = default;
    [[nodiscard]] auto Connect(const MySQLConfig& ConfigParam) -> bool;
    auto Disconnect() noexcept -> void;
    [[nodiscard]] auto Reconnect() -> bool;
    [[nodiscard]] auto Ping() -> bool;
    [[nodiscard]] constexpr auto IsConnectionActive() const noexcept -> bool
    {
        return IsConnected;
    }
    [[nodiscard]] auto Execute(const std::string& SqlCommand) -> MySQLResult;
    [[nodiscard]] auto Query(const std::string& SqlQuery) -> MySQLResult;
    [[nodiscard]] auto ExecuteParameterized(std::string_view QueryTemplate, const std::vector<std::string>& ParameterList) -> MySQLResult;
    [[nodiscard]] auto ExecuteBatch(const std::vector<std::string>& SqlStatements) -> std::vector<MySQLResult>;
    [[nodiscard]] auto BeginTransaction() -> bool;
    [[nodiscard]] auto CommitTransaction() -> bool;
    [[nodiscard]] auto RollbackTransaction() -> bool;
    [[nodiscard]] auto GetTransactionGuard() -> std::unique_ptr<TransactionGuard>;
    template<typename Func>
    [[nodiscard]] auto ExecuteTransaction(Func&& TransactionFunc) -> bool;
    [[nodiscard]] auto PrepareStatement(std::string_view SqlQuery) -> std::expected<std::unique_ptr<sql::PreparedStatement>, std::string>;
    [[nodiscard]] auto ExecutePrepared(sql::PreparedStatement* StatementPtr, const std::vector<std::string>& ParameterList) -> MySQLResult;
    [[nodiscard]] auto GetDatabases() -> std::expected<std::vector<std::string>, std::string>;
    [[nodiscard]] auto GetTables(std::string_view DatabaseName = "") -> std::expected<std::vector<std::string>, std::string>;
    [[nodiscard]] auto GetTableStructure(std::string_view TableName) -> std::expected<MySQLResult, std::string>;
    [[nodiscard]] auto GetTableIndexes(std::string_view TableName) -> std::expected<MySQLResult, std::string>;
    [[nodiscard]] auto GetServerVersion() -> std::expected<std::string, std::string>;
    [[nodiscard]] auto GetCurrentDatabase() -> std::expected<std::string, std::string>;
    [[nodiscard]] auto EscapeString(const std::string& InputString) const -> std::string;
    [[nodiscard]] auto ValidateSQL(std::string_view SqlQuery) const -> bool;
    auto SetQueryTimeout(unsigned int TimeoutSeconds) -> void;
    auto SetResultLimit(std::size_t MaxRows) -> void;
    [[nodiscard]] auto GetLastError() const -> std::string;
    auto SetLogCallback(std::function<void(std::string_view)> CallbackFunc) -> void;
    auto Log(std::string_view Message) const -> void;
    [[nodiscard]] auto ConnectExpected(const MySQLConfig& ConfigParam) -> std::expected<void, std::string>;
    [[nodiscard]] auto QueryExpected(const std::string& SqlQuery) -> std::expected<MySQLResult, std::string>;
    [[nodiscard]] auto GetStatistics() const -> std::tuple<uint64_t, uint64_t, uint64_t>;
    auto ResetStatistics() -> void;
    [[nodiscard]] auto GetConnectionInfo() const -> std::string;
private:
    auto UpdateStatistics(bool IsSuccess) noexcept -> void;
    auto LogError(std::string_view ErrorMessage) -> void;
};

template<typename T>
auto MySQLRow::GetValue(std::size_t Index) const -> std::optional<T>
{
    if (Index >= Fields.size() || Fields[Index] == "NULL") [[unlikely]]
        return std::nullopt;
    try
    {
        if constexpr (std::is_same_v<T, int>)
            return std::stoi(Fields[Index]);
        else if constexpr (std::is_same_v<T, long>)
            return std::stol(Fields[Index]);
        else if constexpr (std::is_same_v<T, long long>)
            return std::stoll(Fields[Index]);
        else if constexpr (std::is_same_v<T, float>)
            return std::stof(Fields[Index]);
        else if constexpr (std::is_same_v<T, double>)
            return std::stod(Fields[Index]);
        else if constexpr (std::is_same_v<T, bool>)
        {
            const auto& FieldValue = Fields[Index];
            return FieldValue == "1" || FieldValue == "true" || FieldValue == "TRUE";
        }
        else if constexpr (std::is_same_v<T, std::string>)
            return Fields[Index];
        else
            static_assert(sizeof(T) == 0, "不支持的类型");
    }
    catch (...)
    {
        return std::nullopt;
    }
}

template<typename Func>
auto MySQLWrapper::ExecuteTransaction(Func&& TransactionFunc) -> bool
{
    std::lock_guard<std::mutex> Lock(ConnectionMutex);
    if (!BeginTransaction()) [[unlikely]]
        return false;
    try
    {
        if (std::forward<Func>(TransactionFunc)())
            return CommitTransaction();
        else
        {
            RollbackTransaction();
            return false;
        }
    }
    catch (...)
    {
        RollbackTransaction();
        return false;
    }
}