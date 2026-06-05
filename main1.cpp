#include <iostream>
#include <QApplication>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QScrollArea>
#include <QFileDialog>
#include <QPixmap>
#include <QFileInfo>
#include <QIntValidator>
#include <QtConcurrent/QtConcurrent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMovie>
#include <QJsonArray>
#include <vips/vips8>


using namespace vips;

QString selectedPath;


// Convert one image with VIPS
static bool convertOne(const QString &srcPath, const QString &dstPath, const int outW, const int outH, const bool toWebp, const int webpQuality){
    try {
        const VImage image = VImage::new_from_file(srcPath.toUtf8().constData(), VImage::option()->set("access", "sequential"));

        const double hscale = static_cast<double>(outW) / image.width();
        const double vscale = static_cast<double>(outH) / image.height();
        const VImage resized = image.resize(std::min(hscale, vscale));

        const QFileInfo fi(srcPath);
        const QString outSuffix = toWebp ? "webp" : fi.suffix().toLower();
        const QString outFile = dstPath + "." + outSuffix;

        if (toWebp) {
            resized.write_to_file(outFile.toUtf8().constData(), VImage::option()->set("Q", webpQuality));
        } else {
            resized.write_to_file(outFile.toUtf8().constData());
        }

        return true;
    } catch (const VError &e) {
        qWarning("libvips error: %s", e.what());
        return false;
    }
}

// A single size-row widget containing: width field, height field, and a remove button.
class SizeRow final : public QWidget {
    Q_OBJECT

public:
    explicit SizeRow(QWidget *parent = nullptr) : QWidget(parent) {
        auto *rowLayout = new QHBoxLayout(this);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        widthField = new QLineEdit(this);
        heightField = new QLineEdit(this);
        removeButton = new QPushButton("x", this);

        widthField->setValidator(new QIntValidator(1, 10000, rowLayout));
        heightField->setValidator(new QIntValidator(1, 10000, rowLayout));
        widthField->setFixedHeight(24);
        heightField->setFixedHeight(24);
        widthField->setTextMargins(5, 0, 5, 0);
        heightField->setTextMargins(5, 0, 5, 0);

        rowLayout->addWidget(widthField);
        rowLayout->addWidget(heightField);
        rowLayout->addWidget(removeButton);
    }

    QLineEdit *widthField;
    QLineEdit *heightField;
    QPushButton *removeButton;
};

// Utility to enable/disable convert button based on selections and sizes
static void updateConvertButtonState(QPushButton *convertButton, const QVBoxLayout *sizeListLayout) {
    bool hasValidSize = sizeListLayout->count() > 0;

    for (int i = 0; i < sizeListLayout->count(); ++i) {
        QWidget *w = sizeListLayout->itemAt(i)->widget();

        const auto *sizeRow = qobject_cast<SizeRow*>(w);
        if (!sizeRow) continue;

        const QString wVal = sizeRow->widthField->text();
        const QString hVal = sizeRow->heightField->text();
        if (wVal.isEmpty() || hVal.isEmpty() || wVal.toInt() <= 0 || wVal.toInt() > 10000 || hVal.toInt() <= 0 || hVal.toInt() > 10000) {
            hasValidSize = false;
        }
    }

    convertButton->setEnabled(!selectedPath.isEmpty() && hasValidSize);
}

// Freeze/unfreeze UI except for cancel button
static void setUIEnabled(const QWidget *parent, const bool enabled, const QPushButton *cancelButton) {
    const QList<QWidget*> widgets = parent->findChildren<QWidget*>();
    for (QWidget *w : widgets) {
        if (w != cancelButton)
            w->setEnabled(enabled);
    }
}

// Adds a row to the layout and wires removal logic.
static SizeRow* createSizeRow(QVBoxLayout *listLayout, const std::function<void()>& updateButtonCallback, const int w = -1, const int h = -1) {
    auto *row = new SizeRow();
    if (w > 0) row->widthField->setText(QString::number(w));
    if (h > 0) row->heightField->setText(QString::number(h));

    listLayout->insertWidget(0, row);

    QObject::connect(row->removeButton, &QPushButton::clicked, [listLayout, row, updateButtonCallback]() {
        listLayout->removeWidget(row);
        row->deleteLater();
        updateButtonCallback();
    });

    QObject::connect(row->widthField, &QLineEdit::textChanged, updateButtonCallback);
    QObject::connect(row->heightField, &QLineEdit::textChanged, updateButtonCallback);

    return row;
}


int main(int argc, char *argv[]) {
    if (VIPS_INIT(argv[0])) vips_error_exit(nullptr);

    QApplication app(argc, argv);

    QApplication::setApplicationName("Webify");
    QApplication::setApplicationDisplayName("Webify");
    QApplication::setApplicationVersion("1.0");

    QWidget window;
    window.setWindowTitle("Webify");
    window.setFixedSize(800, 450);

    auto *rootLayout = new QVBoxLayout(&window);
    rootLayout->setContentsMargins(18, 16, 18, 14);

    auto *topLayer = new QHBoxLayout();
    rootLayout->addLayout(topLayer);

    // LEFT SECTION — File picker
    auto *leftBox = new QGroupBox();
    leftBox->setFixedWidth(180);
    leftBox->setContentsMargins(0, 0, 0, 5);
    auto *leftLayout = new QVBoxLayout(leftBox);

    auto *previewLabel = new QLabel(leftBox);
    previewLabel->setAlignment(Qt::AlignCenter);
    previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    previewLabel->setFixedHeight(90);

    const QPixmap folderIcon("../icons/folder.tiff");
    const QPixmap fileIcon("../icons/file.tiff");

    previewLabel->setPixmap(fileIcon.scaled(80, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    auto *selectedName = new QLabel("Nothing Selected", leftBox);
    selectedName->setAlignment(Qt::AlignCenter);

    auto *directoryModeToggle = new QCheckBox("Directory Mode", leftBox);
    auto *filePickerButton = new QPushButton("Pick File", leftBox);

    leftLayout->addWidget(previewLabel);
    leftLayout->addWidget(selectedName);
    leftLayout->addStretch();
    leftLayout->addWidget(directoryModeToggle);
    leftLayout->addWidget(filePickerButton);

    topLayer->addWidget(leftBox);


    // MIDDLE SECTION — Sizes list
    auto *midBox = new QGroupBox();
    auto *midLayout = new QVBoxLayout(midBox);

    auto *headerRow = new QWidget();
    auto *headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(20, 0, 0, 0);
    headerLayout->setSpacing(100);

    headerLayout->addWidget(new QLabel("Width"));
    headerLayout->addWidget(new QLabel("Height"));

    auto *removeHeader = new QLabel("X");
    removeHeader->setContentsMargins(5, 0, 0, 0);
    headerLayout->addWidget(removeHeader);

    midLayout->addWidget(headerRow);

    auto *scrollArea = new QScrollArea(midBox);
    scrollArea->setWidgetResizable(true);

    auto *sizeContainer = new QWidget();
    auto *sizeListLayout = new QVBoxLayout(sizeContainer);
    sizeListLayout->setAlignment(Qt::AlignTop);

    scrollArea->setWidget(sizeContainer);
    midLayout->addWidget(scrollArea);

    auto *addSizeButton = new QPushButton("Add Size", midBox);
    midLayout->addWidget(addSizeButton);

    topLayer->addWidget(midBox);


    // RIGHT SECTION — Options
    auto *optionsBox = new QGroupBox();
    optionsBox->setContentsMargins(0, -2, 0, 5);
    optionsBox->setFixedWidth(180);
    auto *optionsLayout = new QVBoxLayout(optionsBox);

    auto *convertToWebp = new QCheckBox("Convert to WebP", optionsBox);
    auto *includeOriginal = new QCheckBox("Include Originals", optionsBox);

    auto *checkboxLayout = new QVBoxLayout();
    checkboxLayout->addWidget(convertToWebp);
    checkboxLayout->addWidget(includeOriginal);
    checkboxLayout->setSpacing(15);

    auto *targetDirInfoLabel = new QLabel(
        "By default, all images are saved in the original directory. You can choose a different destination below.",
        optionsBox
    );
    targetDirInfoLabel->setWordWrap(true);

    auto *targetDirLabel = new QLabel("No Directory Selected", optionsBox);
    targetDirLabel->setWordWrap(true);

    auto *targetDirBtn = new QPushButton("Select Save Directory", optionsBox);

    // WebP quality field
    auto *webpQualityLabel = new QLabel("WebP Quality (1-100):", optionsBox);
    auto *webpQualityField = new QLineEdit(optionsBox);
    webpQualityField->setValidator(new QIntValidator(1, 100, optionsBox));
    webpQualityField->setText("90");
    webpQualityField->setFixedHeight(24);
    webpQualityField->setTextMargins(5, 0, 5, 0);

    optionsLayout->addLayout(checkboxLayout);
    optionsLayout->addWidget(webpQualityLabel);
    optionsLayout->addWidget(webpQualityField);
    optionsLayout->addWidget(targetDirInfoLabel);
    optionsLayout->addStretch();
    optionsLayout->addWidget(targetDirLabel);
    optionsLayout->addWidget(targetDirBtn);

    topLayer->addWidget(optionsBox);


    // BOTTOM LAYER
    auto *bottomLayer = new QHBoxLayout();
    auto *saveDefaultsBottom = new QPushButton("Save Settings as Default");
    saveDefaultsBottom->setFixedWidth(180);

    auto *convertButton = new QPushButton("Convert");
    convertButton->setFixedWidth(180);
    auto *cancelButton = new QPushButton("Cancel");
    cancelButton->setFixedWidth(180);
    cancelButton->setVisible(false); // initially invisible

    // Working indicator
    auto *spinnerLabel = new QLabel();
    spinnerLabel->setFixedSize(20, 20);
    spinnerLabel->setContentsMargins(0, 3, 0, 0);
    spinnerLabel->setVisible(false); // initially invisible

    auto *spinnerMovie = new QMovie("../icons/spinner.gif");
    spinnerMovie->setScaledSize(QSize(20, 20));
    spinnerLabel->setMovie(spinnerMovie);

    bottomLayer->addWidget(saveDefaultsBottom, 0, Qt::AlignLeft);
    bottomLayer->addStretch();
    bottomLayer->addWidget(spinnerLabel, 0, Qt::AlignRight);
    bottomLayer->addWidget(cancelButton, 0, Qt::AlignRight);
    bottomLayer->addWidget(convertButton, 0, Qt::AlignRight);

    rootLayout->addLayout(bottomLayer);


    auto updateConvertLambda = [&]() {
        updateConvertButtonState(convertButton, sizeListLayout);
    };


    // Connections

    // Switch file/directory mode
    QObject::connect(directoryModeToggle, &QCheckBox::toggled, [&](const bool checked) {
        filePickerButton->setText(checked ? "Pick Directory" : "Pick File");

        previewLabel->setPixmap((checked ? folderIcon : fileIcon).scaled(
            previewLabel->width(),
            previewLabel->height(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        ));

        selectedName->setText("Nothing selected");
    });

    // File selection
    QObject::connect(filePickerButton, &QPushButton::clicked, [&]() {
        if (directoryModeToggle->isChecked()) {
            selectedPath = QFileDialog::getExistingDirectory(&window, "Select Directory", QDir::homePath());
        } else {
            selectedPath = QFileDialog::getOpenFileName(
                &window,
                "Select Image File",
                QDir::homePath(),
                "Images (*.png *.jpg *.jpeg *.webp);;All Files (*)"
            );
        }

        if (selectedPath.isEmpty())
            return;

        const QFileInfo info(selectedPath);
        selectedName->setText(info.fileName());

        const QPixmap &icon = info.isDir() ? folderIcon : fileIcon;
        previewLabel->setPixmap(icon.scaled(
            previewLabel->width(),
            previewLabel->height(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        ));

        updateConvertLambda();
    });

    // Add-size button
    QObject::connect(addSizeButton, &QPushButton::clicked, [=]() {
        const auto *row = createSizeRow(sizeListLayout, updateConvertLambda);
        Q_UNUSED(row);
        updateConvertLambda();
    });

    // Select save directory
    QObject::connect(targetDirBtn, &QPushButton::clicked, [&]() {
        const QString dir = QFileDialog::getExistingDirectory(&window, "Select Target Directory", QDir::homePath());
        if (!dir.isEmpty()) {
            targetDirLabel->setText(dir);
            targetDirLabel->setStyleSheet("opacity: 1;");
        }
    });

    // Conversion task
    QFuture<void> conversionFuture;
    QObject::connect(convertButton, &QPushButton::clicked, [&]() {
        setUIEnabled(&window, false, cancelButton);
        spinnerLabel->setVisible(true);
        spinnerMovie->start();
        convertButton->setVisible(false);
        cancelButton->setVisible(true);

        const bool toWebp = convertToWebp->isChecked();
        const bool includeOriginalChecked = includeOriginal->isChecked();
        const int webpQuality = webpQualityField->text().toInt();
        const QString overrideDir = targetDirLabel->text().startsWith("/") ? targetDirLabel->text() : QString();
        const QString srcPath = selectedPath;
        if (srcPath.isEmpty()) return;

        conversionFuture = QtConcurrent::run([&window, &conversionFuture, srcPath, sizeListLayout, cancelButton, overrideDir,
                                     includeOriginalChecked, toWebp, webpQuality, convertButton, spinnerMovie, spinnerLabel]() {
            const QFileInfo srcInfo(srcPath);
            QList<QString> paths;
            if (srcInfo.isDir()) {
                const QDir dir(srcPath);
                const QStringList files = dir.entryList({"*.png","*.jpg","*.jpeg","*.webp", "*.bmp", "*.tiff"}, QDir::Files);
                for (const QString &f : files) paths.append(dir.filePath(f));
            } else {
                paths.append(srcPath);
            }

            for (const QString &path : paths) {
                if (conversionFuture.isCanceled()) break; // handle cancel

                QFileInfo fi(path);
                QString baseOutDir = overrideDir.isEmpty() ? fi.absolutePath() : overrideDir;

                QString originalsFolder = baseOutDir + "/Originals";
                if (includeOriginalChecked) QDir().mkpath(originalsFolder);
                QString origOutBase = originalsFolder + "/" + fi.completeBaseName();

                if (includeOriginalChecked) {
                    if (toWebp) {
                        convertOne(path, origOutBase, fi.size(), fi.size(), true, webpQuality);
                    } else {
                        QFile::copy(path, origOutBase + "." + fi.suffix().toLower());
                    }
                }

                for (int i = 0; i < sizeListLayout->count(); ++i) {
                    QWidget *w = sizeListLayout->itemAt(i)->widget();
                    const auto *row = qobject_cast<SizeRow*>(w);
                    if (!row) continue;

                    const int wOut = row->widthField->text().toInt();
                    const int hOut = row->heightField->text().toInt();
                    if (wOut <= 0 || hOut <= 0) continue;

                    QString sizeFolder = QString("%1/%2x%3").arg(baseOutDir).arg(wOut).arg(hOut);
                    QDir().mkpath(sizeFolder);

                    QString outBase = sizeFolder + "/" + fi.completeBaseName();
                    convertOne(path, outBase, wOut, hOut, toWebp, webpQuality);
                }
            }

            QMetaObject::invokeMethod(&window, [&window, cancelButton, convertButton, spinnerMovie, spinnerLabel]() {
                spinnerMovie->stop();
                spinnerLabel->setVisible(false);

                setUIEnabled(&window, true, cancelButton);
                cancelButton->setVisible(false);
                convertButton->setVisible(true);
            });
        });
    });

    // Save all settings as JSON
    QObject::connect(saveDefaultsBottom, &QPushButton::clicked, [&]() {
        QJsonObject rootJson;
        rootJson["lastDirectory"] = selectedPath.isEmpty() ? QDir::homePath() : selectedPath;
        rootJson["lastMode"] = directoryModeToggle->isChecked();
        rootJson["convertToWebp"] = convertToWebp->isChecked();
        rootJson["includeOriginal"] = includeOriginal->isChecked();
        rootJson["targetDirectory"] = targetDirLabel->text();
        rootJson["webpQuality"] = webpQualityField->text().toInt();

        QJsonArray sizesArray;
        for (int i = 0; i < sizeListLayout->count(); ++i) {
            QWidget *w = sizeListLayout->itemAt(i)->widget();
            const auto *row = qobject_cast<SizeRow *>(w);
            if (!row) continue;
            QJsonObject sizeObj;
            sizeObj["width"] = row->widthField->text().toInt();
            sizeObj["height"] = row->heightField->text().toInt();
            sizesArray.append(sizeObj);
        }
        rootJson["sizes"] = sizesArray;

        QFile file("settings.json");
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(QJsonDocument(rootJson).toJson());
        }
    });

    // Cancel button
    QObject::connect(cancelButton, &QPushButton::clicked, [&]() {
        if (conversionFuture.isRunning()) {
            conversionFuture.cancel();
        }
        spinnerMovie->stop();
        spinnerLabel->setVisible(false);
    });


    // Load default image sizes and settings
    {
        QFile file("settings.json");
        bool loaded = false;
        if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) {
                QJsonObject rootJson = doc.object();

                convertToWebp->setChecked(rootJson["convertToWebp"].toBool());
                includeOriginal->setChecked(rootJson["includeOriginal"].toBool());
                if (!rootJson["webpQuality"].isNull()) webpQualityField->setText(QString::number(rootJson["webpQuality"].toInt()));

                QString dir = rootJson["targetDirectory"].toString();
                if (!dir.isEmpty()) targetDirLabel->setText(dir);

                QJsonArray sizesArray = rootJson["sizes"].toArray();
                for (auto sizeVal : sizesArray) {
                    QJsonObject obj = sizeVal.toObject();
                    const int w = obj["width"].toInt();
                    const int h = obj["height"].toInt();
                    if (w > 0 && h > 0) {
                        createSizeRow(sizeListLayout, updateConvertLambda, w, h);
                        loaded = true;
                    }
                }

                // Restore mode first
                bool lastMode = rootJson["lastMode"].toBool();
                directoryModeToggle->setChecked(lastMode);
                filePickerButton->setText(lastMode ? "Pick Directory" : "Pick File");

                // Then restore last path and icon
                QString lastDir = rootJson["lastDirectory"].toString();
                if (!lastDir.isEmpty()) {
                    selectedPath = lastDir;
                    QFileInfo info(selectedPath);
                    selectedName->setText(info.fileName());

                    const QPixmap icon = info.isDir() ? folderIcon : fileIcon;
                    QTimer::singleShot(0, [previewLabel, icon]() {
                        previewLabel->setPixmap(icon.scaled(
                            previewLabel->width(),
                            previewLabel->height(),
                            Qt::KeepAspectRatio,
                            Qt::SmoothTransformation
                        ));
                    });
                }
            }
        }

        if (!loaded) {
            createSizeRow(sizeListLayout, updateConvertLambda, 480, 800);
            createSizeRow(sizeListLayout, updateConvertLambda, 768, 1024);
            createSizeRow(sizeListLayout, updateConvertLambda, 1366, 768);
            createSizeRow(sizeListLayout, updateConvertLambda, 1920, 1080);
        }

        updateConvertLambda();
    }


    window.show();
    const int exitCode = QApplication::exec();

    vips_shutdown();
    return exitCode;
}

#include "main.moc"
