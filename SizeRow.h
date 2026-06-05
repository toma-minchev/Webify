#ifndef WEBIFY_SIZEROW_H
#define WEBIFY_SIZEROW_H


#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QIntValidator>


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


#endif //WEBIFY_SIZEROW_H
