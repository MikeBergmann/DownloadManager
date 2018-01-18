/*
  Copyright 2014 Mike Bergmann, Bones AG

  This file is part of WoodscanDownloadManager.

  WoodscanDownloadManager is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  WoodscanDownloadManager is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with WoodscanDownloadManager.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <QObject>
#include <QNetworkReply>

class QFile;
class QTimer;

/**
 * Download interface class to allow DownloadManager access to
 * protected members.
 */
class DownloadBase : public QObject {

  friend class DownloadManager;

public:
  explicit DownloadBase(QObject *parent = 0);
  virtual ~DownloadBase(void);

protected:
  QNetworkRequest *m_request;
  QNetworkReply *m_reply;

  virtual void stop(void) = 0;
  virtual void fillRequestHeader(void) = 0;
  virtual void parseHeader(void) = 0;
  virtual void openFile(void) = 0;
  virtual QString closeFile(void) = 0;
  virtual bool checkRelocation(void) = 0;
  virtual void relocate(void) = 0;
  virtual int processDownload(qint64 bytesReceived, qint64 bytesTotal, int *percentage) = 0;
  virtual void processFinished(void) = 0;
  virtual void setError(QNetworkReply::NetworkError error) = 0;
};

/**
 * Download class used as handle for DownloadManager
 */
class Download : public DownloadBase {
  Q_OBJECT

public:
  explicit Download(QUrl &url, QDataStream *stream = 0, QObject *parent = 0);
  explicit Download(QUrl &url,const QString &destinationPath, QObject *parent = 0);
  virtual ~Download(void);

  void timeoutTimerStart(void);
  void timeoutTimerStop(void);

  QNetworkReply::NetworkError error(void);
  int errorCnt(void);
  QString filename(void);
  qint64 filesize(void);
  qint64 cursize(void);

signals:
  void timeout(QNetworkReply *reply);

protected:
  void stop(void);
  void fillRequestHeader(void);
  void parseHeader(void);
  void openFile(void);
  QString closeFile(void);
  bool checkRelocation(void);
  void relocate(void);
  int processDownload(qint64 bytesReceived, qint64 bytesTotal, int *percentage);
  void processFinished(void);
  void setError(QNetworkReply::NetworkError error);

private slots:
  void timeout(void);

private:
  QString m_destinationPath;
  QString m_fileName;
  QFile *m_file;
  QDataStream *m_stream;

  bool m_hostSupportsRanges;
  qint64 m_totalSize;
  qint64 m_downloadSize;
  qint64 m_pausedSize;
  QNetworkReply::NetworkError m_error;
  int m_errorCnt;

  QString m_newLocation;

  QTimer *m_timer;
};

#endif // DOWNLOAD_H
