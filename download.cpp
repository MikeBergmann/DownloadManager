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

#include "download.h"

#include <QNetworkRequest>
#include <QString>
#include <QTimer>
#include <QDebug>
#include <QFile>

DownloadBase::DownloadBase(QObject *parent)
  : QObject(parent)
  , m_request(0)
  , m_reply(0) {
}

DownloadBase::~DownloadBase()
{
}

Download::Download(QUrl &url, QDataStream *stream, QObject *parent)
  : DownloadBase(parent)
  , m_destinationPath()
  , m_fileName()
  , m_file(0)
  , m_stream(stream)
  , m_hostSupportsRanges(false)
  , m_totalSize(0)
  , m_downloadSize(0)
  , m_pausedSize(0)
  , m_error(QNetworkReply::NoError)
  , m_errorCnt(0)
  , m_newLocation()
  , m_timer(0)
{
  m_request = new QNetworkRequest();
  m_request->setUrl(url);

  m_downloadSize = 0;
  m_pausedSize = 0;

  // Timeout timer
  m_timer = new QTimer(this);
  m_timer->setInterval(15000);
  m_timer->setSingleShot(true);
}

Download::Download(QUrl &url, const QString &destinationPath, QObject *parent)
  : Download(url, 0, parent)
{
  m_destinationPath = destinationPath;
}

Download::~Download()
{
  try {
    if(m_reply) {
      stop();
    }
    delete m_request;
    closeFile();
  } catch(...) {
    qDebug() << "exception within ~Download()" << endl;
  }
}

void Download::stop()
{
  timeoutTimerStop();

  if(m_reply) {
    m_reply->abort();
    m_reply->deleteLater();
    m_reply = 0;
  }

  if(m_file) {
    m_file->flush();
  }

  m_pausedSize = m_downloadSize;
}

void Download::fillRequestHeader()
{
  if(m_hostSupportsRanges) {
    QByteArray rangeHeaderValue = "bytes=" + QByteArray::number(m_pausedSize) + "-";
    if(m_totalSize > 0) {
      rangeHeaderValue += QByteArray::number(m_totalSize);
    }
    m_request->setRawHeader("Range", rangeHeaderValue);
  }

  m_request->setRawHeader("Connection", "Keep-Alive");
  m_request->setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
}

void Download::parseHeader()
{
  m_hostSupportsRanges = false;
  m_totalSize = 0;
  m_fileName.clear();
  m_newLocation.clear();

  if(m_reply->hasRawHeader("Accept-Ranges")) {
    QString acceptRanges = m_reply->rawHeader("Accept-Ranges");
    m_hostSupportsRanges = (acceptRanges.compare("bytes", Qt::CaseInsensitive) == 0);
    qDebug() << "Accept-Ranges = " << acceptRanges << m_hostSupportsRanges;
  }

  if(m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong() > 0) {
    m_totalSize = m_reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
  }

  QString disposition;
  if(m_reply->hasRawHeader("Content-Disposition")) {
    disposition = m_reply->rawHeader("Content-Disposition");
  }

  if(!disposition.isEmpty()) {
    m_fileName = QString(disposition.split("filename=").at(1)).remove("\"");
    if(!m_destinationPath.isEmpty()) {
      m_fileName = m_destinationPath + "/" + m_fileName;
    }
  }

  // Check if we get a relocation, this will happen if we request a download by a php script.
  if(m_reply->hasRawHeader("Location")) {
    m_newLocation = m_reply->rawHeader("Location");
  }

  m_error = m_reply->error();
  if(m_error != QNetworkReply::NoError) {
    m_errorCnt++;
  }
}

void Download::openFile()
{
  // Only open the file if we have no valid stream, otherwise
  // the object was created with an valid stream to write the data to,
  // or the method was called before
  if(!m_stream) {
    m_file = new QFile(m_fileName + ".part");
    if(!m_hostSupportsRanges) {
      m_file->remove();
    }
    m_file->open(QIODevice::ReadWrite | QIODevice::Append);
    m_stream = new QDataStream(m_file);
    m_pausedSize = m_file->size();
  }
}

QString Download::closeFile()
{
  if(m_file) {
    if(m_file->isOpen()) {
      m_file->close();
    }
    if(!m_fileName.isEmpty()) {
      if(m_error == QNetworkReply::NoError) {
        QFile::remove(m_fileName);
        m_file->rename(m_fileName + ".part", m_fileName);
      } else {
        QFile::remove(m_fileName);
        QFile::remove(m_fileName + ".part");
      }
    }
    delete m_file;
    m_file = 0;
  }

  delete m_stream;
  m_stream = 0;

  return m_fileName;
}

bool Download::checkRelocation()
{
  return !m_newLocation.isEmpty();
}

void Download::relocate()
{
  // We got a new location of the file, recreate request
 QUrl url = m_request->url().scheme()+"://"+m_request->url().host()+"/"+m_newLocation;
 delete m_request;

 m_request = new QNetworkRequest();
 m_request->setUrl(url);
}

void Download::timeoutTimerStart()
{
  connect(m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
  m_timer->start();
}

void Download::timeoutTimerStop()
{
  disconnect(m_timer, SIGNAL(timeout()), this, SLOT(timeout()));
  m_timer->stop();
}

int Download::processDownload(qint64 bytesReceived, qint64 bytesTotal, int *percentage)
{
  m_downloadSize = m_pausedSize + bytesReceived;
  qDebug() << QTime::currentTime() << "Download Progress: Received=" << m_downloadSize << ": Total=" << m_pausedSize + bytesTotal;

  if(!m_reply) {
    return false;
  }

  QByteArray replyData = m_reply->readAll();

  // Do not stream to QDataStream because the stream makes some sort of
  // data encoding, use writeRawData instead.
  if(m_stream->writeRawData(replyData.data(),replyData.size()) < replyData.size()) {
    if(m_file) {
      qDebug() << "File size:" << m_file->size() << endl;
      qDebug() << "File error:" << m_file->error() << m_file->errorString() << endl;
    }
    return false;
  }

  // Make sure we have no problems because of caching.
  if(m_file) {
    m_stream->device()->close();
    if(!m_stream->device()->open(QIODevice::Append)) {
      qDebug() << "Can not reopen file:" << m_file->error() << m_file->errorString() << endl;
      return false;
    }
  }

  m_error = m_reply->error();
  if(m_error != QNetworkReply::NoError) {
    m_errorCnt++;
  }

  *percentage = static_cast<int>((static_cast<float>(m_pausedSize + bytesReceived) * 100.0) / static_cast<float>(m_pausedSize + bytesTotal));

  if(m_totalSize == 0 && bytesTotal > 0) {
    m_totalSize = m_pausedSize + bytesTotal;
  }

  return true;
}

void Download::processFinished()
{
  if(m_reply  == 0) {
    return;
  }

  m_error = m_reply->error();
  if(m_error != QNetworkReply::NoError) {
    m_errorCnt++;
  }
}

QNetworkReply::NetworkError Download::error()
{
  return m_error;
}

void Download::setError(QNetworkReply::NetworkError error)
{
  m_error = error;
}

int Download::errorCnt()
{
  return m_errorCnt;
}

QString Download::filename()
{
  return m_fileName;
}

qint64 Download::filesize()
{
  return m_totalSize;
}

qint64 Download::cursize()
{
  return m_downloadSize;
}

void Download::timeout()
{
  emit timeout(m_reply);
}
