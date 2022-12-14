#pragma once

#include <QString>
#include <QVariant>

#include <sqlite3.h>

#define DEFAULT_TABLE_NAME "METRICS"
QString tableFromQuery(const QString &query);

struct Importer;

struct Db {
	~Db() { close_db(); }
    // We can import multiple csv, or json files at the same time. Only one sqlite db though.
    QStringList dbFiles;
	sqlite3 *db=nullptr;
	void open_db();
	void close_db();
	void save_db(const QString &filePath) const;

	QString errorMsg; // same as errorMsg of the last executed Stmt

	struct Stmt {
		Db &db;
		const QString query;
		sqlite3_stmt *stmt;

		QString errorMsg; // errorMsg.isNull() if the query went fine.

        ~Stmt();

        int exec();
        void bind(int i, const QString &value);

        int column_count() const;
        unsigned row_count(); // WARNING: consumes the data
        QStringList column_names() const;

        QList<QVariant> next_row();

        friend struct Db;
        private:
		Stmt(Db &db_, const QString &query_, int verbosity);
	};
	Stmt query(const QString &query, int verbosity=1);
    QStringList distinctValues(const QString &column, const QString &table, int verbosity=1);
    unsigned row_count(const QString &table, const QString &where_clause="1=1");
    bool table_exists(const QString &tableName);

public:
    void import(const QString &filePath, Importer *importer);
};

struct Importer {
    virtual QString name() const =0;
    virtual QStringList patterns() const = 0;
    virtual QString tableName() const = 0;

    virtual bool canHandleFilePath(const QString &filePath) = 0;

    virtual void openFile(const QString &filePath) = 0;
    virtual QStringList columnNames() = 0;
    virtual void readAllRows() = 0;
    virtual void closeFile() { }

private:
    Db *db;
	friend struct Db;
    QStringList _columnNames;
    QStringList _columnsParams; // just a series of question marks
    QString _tableName;
protected:
	void insertRow(const QStringList &data);
};

extern QVector<Importer*> importers;
void init_importers();
