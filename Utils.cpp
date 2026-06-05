#include "Utils.h"
#include "SizeRow.h"
#include <QObject>

void updateConvertButtonState(QPushButton *convertButton, const QVBoxLayout *sizeListLayout) {
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

void setUIEnabled(const QWidget *parent, const bool enabled, const QPushButton *cancelButton) {
    const QList<QWidget*> widgets = parent->findChildren<QWidget*>();
    for (QWidget *w : widgets) {
        if (w != cancelButton)
            w->setEnabled(enabled);
    }
}

SizeRow* createSizeRow(QVBoxLayout *listLayout, const std::function<void()>& updateButtonCallback, const int w, const int h) {
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