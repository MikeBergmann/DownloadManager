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

#ifndef DOWNLOADMANAGER_H_
#define DOWNLOADMANAGER_H_

#include <QObject>

class Download;
class QNetworkAccessManager;

#include <QNetworkReply>

class DownloadManager : public QObject {
  Q_OBJECT

public:
  explicit DownloadManager(QObject *parent = 0);
  virtual ~DownloadManager(void);

signals:
  void printText(QString qsLine);
  void complete(Download *dl);
  void failed(Download *dl);
  void downloadProgress(Download *dl, int percentage);

public slots:
  Download* download(QUrl url, const QString &destinationPath);
  Download* download(QUrl url, QByteArray *destination);
  void stop(Download *dl);
  void stopAll(void);
  void resume(Download *dl);

private slots:
  void gotHeader(void);
  void finished(void);
  void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
  void gotError(QNetworkReply::NetworkError errorCode);
  void authenticationRequired(QNetworkReply *reply, QAuthenticator *auth);
  void timeout(QNetworkReply* reply);

private:
  QNetworkAccessManager *m_manager;
  QHash<QNetworkReply*, Download*> m_downloads;
  int m_notfincnt;

  void doDownload(Download *dl);
  void downloadRequest(Download *dl);
  void cleanupDownload(Download *dl);
};

#endif // DOWNLOADMANAGER_H_

