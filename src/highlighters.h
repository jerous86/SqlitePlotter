#pragma once

#include <QTextEdit>
#include <QSyntaxHighlighter>
#include <QRegularExpression>

#define REGEX QRegularExpression

class MySqliteHighlighter : public QSyntaxHighlighter {
	Q_OBJECT

public:
	explicit MySqliteHighlighter(QTextEdit *parent);

	void setColumnNames(QStringList columnNames);
	void setVariableNames(QStringList varNames);
protected:
	void highlightBlock(const QString &text) override;

private:
	struct HighlightingRule
	{
		REGEX pattern;
		QTextCharFormat format;
	};
	QVector<HighlightingRule> highlightingRules;

	QTextCharFormat keywordFormat;
	QTextCharFormat singleLineCommentFormat;
	QTextCharFormat unknownVariableFormat;
	QTextCharFormat variableFormat;
	QTextCharFormat quotationFormat;
	QTextCharFormat unknownFunctionFormat;
	QTextCharFormat functionFormat;
	QTextCharFormat columnName;

	int ruleValidVariable, ruleColumnName;
};


class MyBatchSaveHighlighter : public QSyntaxHighlighter {
	Q_OBJECT

public:
	explicit MyBatchSaveHighlighter(QTextEdit *parent);

protected:
	void highlightBlock(const QString &text) override;

private:
	struct HighlightingRule
	{
		REGEX pattern;
		QTextCharFormat format;
	};
	QVector<HighlightingRule> highlightingRules;

	QTextCharFormat pathFormat;
	QTextCharFormat variableFormat;
	QTextCharFormat variableAssignmentFormat;
	QTextCharFormat singleLineCommentFormat;
};

