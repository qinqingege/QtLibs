#ifndef FILECACHE_H
#define FILECACHE_H

#include <QObject>
#include <QMap>
#include <QNetworkReply>

//非线程安全组件！


class Downloader : public QObject
{
	Q_OBJECT
public:
	Downloader(const QString &url, bool cache);

	void run(QNetworkAccessManager *networkAccessManager);
	const QString& getUrl() const;

private slots:
	void onFinished();

signals:
	//如果cache_ == true，filePathOrData是缓存文件路径；
	//否则是文件内容。
	void finished(QNetworkReply::NetworkError error, QString url, QString filePathOrData);

private:
	QString url_;
	bool cache_;
};

class PendingRequest
{
public:
	PendingRequest(const QString &url, QObject *receiver, const char *member, bool cache)
		: url_(url)
		, receiver_(receiver)
		, member_(member)
		, cache_(cache)
	{}

	QString url_;
	QObject *receiver_;
	const char *member_;
	bool cache_;
};

bool operator ==(const PendingRequest &lhs, const PendingRequest &rhs);

class FileCache : public QObject
{
	Q_OBJECT

public:
	~FileCache();

	void init(const QString &cacheDir, QNetworkAccessManager *networkAccessManager);

	static FileCache& instance();

	void setMaxConcurrency(int maxCon);
	bool removeDownload(const QString &url);

	QString getFilePathByUrl(const QString &url, bool downloadIfNoExist, QObject *receiver, const char *member);
	//cache == true：下载内容缓存成文件；
	//否则不缓存。
	void downloadFile(const QString &url, QObject *receiver, const char *member, bool cache);
	QString makeCacheFilePathByUrl(const QString &url);

private slots:
	void onDownloaderFinished(QNetworkReply::NetworkError error, QString, QString);

private:
	FileCache();
	FileCache(const FileCache&);
	FileCache& operator=(const FileCache&);

	QMap<QString, Downloader*> downloading_;
	QList<PendingRequest> pending_;
	int maxConcurrency_;
	QString cacheDir_;
	QNetworkAccessManager *networkAccessManager_;
};


#define FileCacheObject FileCache::instance()

#endif // FILECACHE_H
