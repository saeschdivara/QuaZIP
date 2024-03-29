#include "testquazipfile.h"

#include "qztest.h"

#include <quazip/JlCompress.h>
#include <quazip/quazipfile.h>
#include <quazip/quazip.h>

#include <QFile>
#include <QString>
#include <QStringList>

#include <QtTest/QtTest>

void TestQuaZipFile::zipUnzip_data()
{
    QTest::addColumn<QString>("zipName");
    QTest::addColumn<QStringList>("fileNames");
    QTest::addColumn<QByteArray>("fileNameCodec");
    QTest::addColumn<QByteArray>("password");
    QTest::addColumn<bool>("zip64");
    QTest::newRow("simple") << "simple.zip" << (
            QStringList() << "test0.txt" << "testdir1/test1.txt"
            << "testdir2/test2.txt" << "testdir2/subdir/test2sub.txt")
        << QByteArray() << QByteArray() << false;
    QTest::newRow("Cyrillic") << "cyrillic.zip" << (
            QStringList()
            << QString::fromUtf8("русское имя файла с пробелами.txt"))
        << QByteArray("IBM866") << QByteArray() << false;
    QTest::newRow("password") << "password.zip" << (
            QStringList() << "test.txt")
        << QByteArray() << QByteArray("PassPass") << false;
    QTest::newRow("zip64") << "zip64.zip" << (
            QStringList() << "test64.txt")
        << QByteArray() << QByteArray() << true;
}

void TestQuaZipFile::zipUnzip()
{
    QFETCH(QString, zipName);
    QFETCH(QStringList, fileNames);
    QFETCH(QByteArray, fileNameCodec);
    QFETCH(QByteArray, password);
    QFETCH(bool, zip64);
    QFile testFile(zipName);
    if (testFile.exists()) {
        if (!testFile.remove()) {
            QFAIL("Couldn't remove existing archive to create a new one");
        }
    }
    if (!createTestFiles(fileNames)) {
        QFAIL("Couldn't create test files for zipping");
    }
    QuaZip testZip(&testFile);
    testZip.setZip64Enabled(zip64);
    if (!fileNameCodec.isEmpty())
        testZip.setFileNameCodec(fileNameCodec);
    QVERIFY(testZip.open(QuaZip::mdCreate));
    QString comment = "Test comment";
    testZip.setComment(comment);
    foreach (QString fileName, fileNames) {
        QFile inFile("tmp/" + fileName);
        if (!inFile.open(QIODevice::ReadOnly)) {
            qDebug("File name: %s", fileName.toUtf8().constData());
            QFAIL("Couldn't open input file");
        }
        QuaZipFile outFile(&testZip);
        QVERIFY(outFile.open(QIODevice::WriteOnly, QuaZipNewInfo(fileName,
                        inFile.fileName()),
                password.isEmpty() ? NULL : password.constData()));
        for (qint64 pos = 0, len = inFile.size(); pos < len; ) {
            char buf[4096];
            qint64 readSize = qMin(static_cast<qint64>(4096), len - pos);
            qint64 l;
            if ((l = inFile.read(buf, readSize)) != readSize) {
                qDebug("Reading %ld bytes from %s at %ld returned %ld",
                        static_cast<long>(readSize),
                        fileName.toUtf8().constData(),
                        static_cast<long>(pos),
                        static_cast<long>(l));
                QFAIL("Read failure");
            }
            QVERIFY(outFile.write(buf, readSize));
            pos += readSize;
        }
        inFile.close();
        outFile.close();
        QCOMPARE(outFile.getZipError(), ZIP_OK);
    }
    testZip.close();
    QCOMPARE(testZip.getZipError(), ZIP_OK);
    // now test unzip
    QuaZip testUnzip(&testFile);
    if (!fileNameCodec.isEmpty())
        testUnzip.setFileNameCodec(fileNameCodec);
    QVERIFY(testUnzip.open(QuaZip::mdUnzip));
    QCOMPARE(testUnzip.getComment(), comment);
    QVERIFY(testUnzip.goToFirstFile());
    foreach (QString fileName, fileNames) {
        QCOMPARE(testUnzip.getCurrentFileName(), fileName);
        QFile original("tmp/" + fileName);
        QVERIFY(original.open(QIODevice::ReadOnly));
        QuaZipFile archived(&testUnzip);
        QVERIFY(archived.open(QIODevice::ReadOnly,
                         password.isEmpty() ? NULL : password.constData()));
        QByteArray originalData = original.readAll();
        QByteArray archivedData = archived.readAll();
        QCOMPARE(archivedData, originalData);
        testUnzip.goToNextFile();
    }
    testUnzip.close();
    QCOMPARE(testUnzip.getZipError(), UNZ_OK);
    // clean up
    removeTestFiles(fileNames);
    testFile.remove();
}

void TestQuaZipFile::bytesAvailable_data()
{
    QTest::addColumn<QString>("zipName");
    QTest::addColumn<QStringList>("fileNames");
    QTest::newRow("simple") << "test.zip" << (
            QStringList() << "test0.txt" << "testdir1/test1.txt"
            << "testdir2/test2.txt" << "testdir2/subdir/test2sub.txt");
}

void TestQuaZipFile::bytesAvailable()
{
    QFETCH(QString, zipName);
    QFETCH(QStringList, fileNames);
    QDir curDir;
    if (!createTestFiles(fileNames)) {
        QFAIL("Couldn't create test files");
    }
    if (!JlCompress::compressDir(zipName, "tmp")) {
        QFAIL("Couldn't create test archive");
    }
    QuaZip testZip(zipName);
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    foreach (QString fileName, fileNames) {
        QFileInfo fileInfo("tmp/" + fileName);
        QVERIFY(testZip.setCurrentFile(fileName));
        QuaZipFile zipFile(&testZip);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        QCOMPARE(zipFile.bytesAvailable(), fileInfo.size());
        QCOMPARE(zipFile.read(1).size(), 1);
        QCOMPARE(zipFile.bytesAvailable(), fileInfo.size() - 1);
        QCOMPARE(zipFile.read(fileInfo.size() - 1).size(),
                static_cast<int>(fileInfo.size() - 1));
        QCOMPARE(zipFile.bytesAvailable(), (qint64) 0);
    }
    removeTestFiles(fileNames);
    testZip.close();
    curDir.remove(zipName);
}

void TestQuaZipFile::atEnd_data()
{
    bytesAvailable_data();
}

void TestQuaZipFile::atEnd()
{
    QFETCH(QString, zipName);
    QFETCH(QStringList, fileNames);
    QDir curDir;
    if (!createTestFiles(fileNames)) {
        QFAIL("Couldn't create test files");
    }
    if (!JlCompress::compressDir(zipName, "tmp")) {
        QFAIL("Couldn't create test archive");
    }
    QuaZip testZip(zipName);
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    foreach (QString fileName, fileNames) {
        QFileInfo fileInfo("tmp/" + fileName);
        QVERIFY(testZip.setCurrentFile(fileName));
        QuaZipFile zipFile(&testZip);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        QCOMPARE(zipFile.atEnd(), false);
        QCOMPARE(zipFile.read(1).size(), 1);
        QCOMPARE(zipFile.atEnd(), false);
        QCOMPARE(zipFile.read(fileInfo.size() - 1).size(),
                static_cast<int>(fileInfo.size() - 1));
        QCOMPARE(zipFile.atEnd(), true);
    }
    removeTestFiles(fileNames);
    testZip.close();
    curDir.remove(zipName);
}

void TestQuaZipFile::pos_data()
{
    bytesAvailable_data();
}

void TestQuaZipFile::pos()
{
    QFETCH(QString, zipName);
    QFETCH(QStringList, fileNames);
    QDir curDir;
    if (!createTestFiles(fileNames)) {
        QFAIL("Couldn't create test files");
    }
    if (!JlCompress::compressDir(zipName, "tmp")) {
        QFAIL("Couldn't create test archive");
    }
    QuaZip testZip(zipName);
    QVERIFY(testZip.open(QuaZip::mdUnzip));
    foreach (QString fileName, fileNames) {
        QFileInfo fileInfo("tmp/" + fileName);
        QVERIFY(testZip.setCurrentFile(fileName));
        QuaZipFile zipFile(&testZip);
        QVERIFY(zipFile.open(QIODevice::ReadOnly));
        QCOMPARE(zipFile.pos(), (qint64) 0);
        QCOMPARE(zipFile.read(1).size(), 1);
        QCOMPARE(zipFile.pos(), (qint64) 1);
        QCOMPARE(zipFile.read(fileInfo.size() - 1).size(),
                static_cast<int>(fileInfo.size() - 1));
        QCOMPARE(zipFile.pos(), fileInfo.size());
    }
    removeTestFiles(fileNames);
    testZip.close();
    curDir.remove(zipName);
}

void TestQuaZipFile::getZip()
{
    QuaZip testZip;
    QuaZipFile f1(&testZip);
    QCOMPARE(f1.getZip(), &testZip);
    QuaZipFile f2("doesntexist.zip", "someFile");
    QCOMPARE(f2.getZip(), static_cast<QuaZip*>(NULL));
    f2.setZip(&testZip);
    QCOMPARE(f2.getZip(), &testZip);
}

void TestQuaZipFile::setZipName()
{
    QString testFileName = "testZipName.txt";
    QString testZipName = "testZipName.zip";
    QVERIFY(createTestFiles(QStringList() << testFileName));
    QVERIFY(createTestArchive(testZipName, QStringList() << testFileName));
    QuaZipFile testFile;
    testFile.setZipName(testZipName);
    QCOMPARE(testFile.getZipName(), testZipName);
    testFile.setFileName(testFileName);
    QVERIFY(testFile.open(QIODevice::ReadOnly));
    testFile.close();
    removeTestFiles(QStringList() << testFileName);
    QDir curDir;
    curDir.remove(testZipName);
}

void TestQuaZipFile::getFileInfo()
{
    QuaZipFileInfo info32;
    QuaZipFileInfo64 info64;
    QString testFileName = "testZipName.txt";
    QStringList testFiles;
    testFiles << testFileName;
    QString testZipName = "testZipName.zip";
    QVERIFY(createTestFiles(testFiles));
    QVERIFY(createTestArchive(testZipName, testFiles));
    QuaZipFile testFile;
    testFile.setZipName(testZipName);
    testFile.setFileName(testFileName);
    QVERIFY(testFile.open(QIODevice::ReadOnly));
    QVERIFY(testFile.getFileInfo(&info32));
    QVERIFY(testFile.getFileInfo(&info64));
    QCOMPARE(info32.name, info64.name);
    QCOMPARE(info32.versionCreated, info64.versionCreated);
    QCOMPARE(info32.versionNeeded, info64.versionNeeded);
    QCOMPARE(info32.flags, info64.flags);
    QCOMPARE(info32.method, info64.method);
    QCOMPARE(info32.dateTime, info64.dateTime);
    QCOMPARE(info32.crc, info64.crc);
    QCOMPARE(info32.compressedSize,
             static_cast<quint32>(info64.compressedSize));
    QCOMPARE(info32.uncompressedSize,
             static_cast<quint32>(info64.uncompressedSize));
    QCOMPARE(info32.diskNumberStart, info64.diskNumberStart);
    QCOMPARE(info32.internalAttr, info64.internalAttr);
    QCOMPARE(info32.externalAttr, info64.externalAttr);
    QCOMPARE(info32.comment, info64.comment);
    QCOMPARE(info32.extra, info64.extra);
    testFile.close();
    removeTestFiles(testFiles);
    QDir curDir;
    curDir.remove(testZipName);
}

void TestQuaZipFile::setFileName()
{
    QString testFileName = "testZipName.txt";
    QString testZipName = "testZipName.zip";
    QVERIFY(createTestFiles(QStringList() << testFileName));
    QVERIFY(createTestArchive(testZipName, QStringList() << testFileName));
    QuaZipFile testFile(testZipName);
    testFile.setFileName(testFileName.toUpper());
#ifdef Q_WS_WIN
    QVERIFY(testFile.open(QIODevice::ReadOnly));
    testFile.close();
#else
    QVERIFY(!testFile.open(QIODevice::ReadOnly));
#endif
    testFile.setFileName(testFileName.toUpper(), QuaZip::csInsensitive);
    QCOMPARE(testFile.getCaseSensitivity(), QuaZip::csInsensitive);
    QVERIFY(testFile.open(QIODevice::ReadOnly));
    QCOMPARE(testFile.getActualFileName(), testFileName);
    testFile.close();
    testFile.setFileName(testFileName.toUpper(), QuaZip::csSensitive);
    QCOMPARE(testFile.getFileName(), testFileName.toUpper());
    QCOMPARE(testFile.getActualFileName(), QString());
    QVERIFY(!testFile.open(QIODevice::ReadOnly));
    testFile.setFileName(testFileName);
    removeTestFiles(QStringList() << testFileName);
    QDir curDir;
    curDir.remove(testZipName);
}

void TestQuaZipFile::constructorDestructor()
{
    // Just test that all constructors and destructors are available.
    // (So there are none that are declared but not defined.)
    QuaZip testZip;
    QuaZipFile *f1 = new QuaZipFile();
    delete f1; // test D0 destructor
    QObject parent;
    QuaZipFile f2(&testZip, &parent);
    QuaZipFile f3(&parent);
    QuaZipFile f4("zipName.zip");
    QuaZipFile f5("zipName.zip", "fileName.txt", QuaZip::csDefault, &parent);
}
