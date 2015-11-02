#include "filecache.h"
#include <QCryptographicHash>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QThread>
#include <QDebug>


bool operator ==(const PendingRequest &lhs, const PendingRequest &rhs)
{
	return lhs.url_ == rhs.url_ && lhs.receiver_ == rhs.receiver_ && strcmp(lhs.member_, rhs.member_) == 0 && lhs.cache_ == rhs.cache_;
}

Downloader::Downloader(const QString &url, bool cache)
	: url_(url)
	, cache_(cache)
{
}

void Downloader::run(QNetworkAccessManager *networkAccessManager)
{
	QNetworkReply *reply = networkAccessManager->get(QNetworkRequest(QUrl(url_)));
	connect(reply, SIGNAL(finished()), this, SLOT(onFinished()));
}

void Downloader::onFinished()
{
	qDebug() << "File is coming : " << url_;
	QNetworkReply *reply = reinterpret_cast<QNetworkReply*>(sender());
	reply->deleteLater();
	if (cache_)
	{
		QString filePath = FileCacheObject.makeCacheFilePathByUrl(url_);
		QFile file(filePath);
		if (file.open(QFile::WriteOnly | QFile::Truncate))
		{
			file.write(reply->readAll());
			file.flush();
			file.close();
		}
		emit finished(reply->error(), url_, filePath);
	} 
	else
	{
		emit finished(reply->error(), url_, QString::fromUtf8(reply->readAll()));
	}
}

const QString& Downloader::getUrl() const
{
	return url_;
}

FileCache::FileCache()
	: QObject(nullptr)
	, networkAccessManager_(nullptr)
{

}

FileCache::~FileCache()
{

}

void FileCache::init(const QString &cacheDir, QNetworkAccessManager *networkAccessManager)
{
	networkAccessManager_ = networkAccessManager;
	cacheDir_ = cacheDir;
	maxConcurrency_ = QThread::idealThreadCount();
	maxConcurrency_ = qMax(2, maxConcurrency_);
	qDebug() << __FUNCTION__ << " maxConcurrency_ = " << maxConcurrency_;
}

FileCache& FileCache::instance()
{
	static FileCache s;
	return s;
}

void FileCache::setMaxConcurrency(int maxCon)
{
	maxConcurrency_ = qMax(maxConcurrency_, maxCon);
	qDebug() << __FUNCTION__ << " maxConcurrency_ = " << maxConcurrency_;
}

bool FileCache::removeDownload(const QString &url)
{
	for (int i = 0; i < pending_.size(); ++i)
	{
		if (pending_[i].url_ == url)
		{
			pending_.removeAt(i);
			return true;
		}
	}
	return false;
}

QString FileCache::getFilePathByUrl(const QString &url, bool downloadIfNoExist, QObject *receiver, const char *member)
{
	QString name = makeCacheFilePathByUrl(url);
	if (QFile::exists(name))
	{
		qDebug() << __FUNCTION__ << ", file from url (" << url << ") exists : " << name;
		return name;
	}

	auto iter = downloading_.find(url);
	if (iter != downloading_.end())
	{
		qDebug() << __FUNCTION__ << " same url is downloading : " << url;

		connect(iter.value(), SIGNAL(finished(QNetworkReply::NetworkError, QString, QString)), receiver, member);
		return "";
	}

	if (pending_.indexOf(PendingRequest(url, receiver, member, true)) != -1)
	{
		qDebug() << __FUNCTION__ << " same url is pending : " << url;
		return "";
	}

	if (downloadIfNoExist)
	{
		downloadFile(url, receiver, member, true);
	}
	return "";
}

void FileCache::downloadFile(const QString &url, QObject *receiver, const char *member, bool cache)
{
	qDebug() << "downloadFile : " << url;
	if (downloading_.size() < maxConcurrency_)
	{
		Downloader *downloader = new Downloader(url, cache);
		connect(downloader, SIGNAL(finished(QNetworkReply::NetworkError, QString, QString)), receiver, member);
		connect(downloader, SIGNAL(finished(QNetworkReply::NetworkError, QString, QString)), this, SLOT(onDownloaderFinished(QNetworkReply::NetworkError, QString, QString)));
		downloading_.insert(url, downloader);
		downloader->run(networkAccessManager_);
	}
	else
	{
		qDebug() << __FUNCTION__ << " push back pending list." << url;
		pending_.push_back(PendingRequest(url, receiver, member, cache));
	}
}

QString FileCache::makeCacheFilePathByUrl(const QString &url)
{
	QString name = cacheDir_;
	if (!name.endsWith('\\'))
	{
		name += "\\";
	}
	name += QCryptographicHash::hash(url.toAscii(), QCryptographicHash::Md5).toHex();
	return name;
}

void FileCache::onDownloaderFinished(QNetworkReply::NetworkError error, QString, QString)
{
	Downloader *d = reinterpret_cast<Downloader*>(sender());
	downloading_.remove(d->getUrl());
	d->deleteLater();
	if (!pending_.empty())
	{
		qDebug() << __FUNCTION__ << " download 1st url of pending list." << pending_[0].url_;
		downloadFile(pending_[0].url_, pending_[0].receiver_, pending_[0].member_, pending_[0].cache_);
		pending_.pop_front();
	}
}