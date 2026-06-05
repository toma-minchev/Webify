#ifndef WEBIFY_UTILS_H
#define WEBIFY_UTILS_H


#include <functional>
#include <QString>
#include <QVBoxLayout>
#include <QPushButton>
#include <QWidget>

class SizeRow;

extern QString selectedPath;

void updateConvertButtonState(QPushButton *convertButton, const QVBoxLayout *sizeListLayout);
void setUIEnabled(const QWidget *parent, bool enabled, const QPushButton *cancelButton);
SizeRow* createSizeRow(QVBoxLayout *listLayout, const std::function<void()>& updateButtonCallback, int w = -1, int h = -1);


#endif //WEBIFY_UTILS_H
