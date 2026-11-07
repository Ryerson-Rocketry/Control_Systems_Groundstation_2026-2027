#include "retropanel.h"
#include "theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

RetroPanel::RetroPanel(const QString &title, const QColor &headerColor, QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("retroPanel"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(3, 3, 3, 3); // inside the 3px border
    outer->setSpacing(0);

    auto *header = new QFrame;
    header->setObjectName(QStringLiteral("retroHeader"));
    header->setStyleSheet(QStringLiteral("QFrame#retroHeader { background: %1; border-bottom: 3px solid %2; }")
                              .arg(headerColor.name(), Theme::Ink.name()));

    auto *headerRow = new QHBoxLayout(header);
    headerRow->setContentsMargins(8, 3, 8, 5);

    auto *titleLabel = new QLabel(title);
    titleLabel->setFont(Theme::pixelFont(13));
    m_info = new QLabel;
    m_info->setFont(Theme::pixelFont(12));

    headerRow->addWidget(titleLabel);
    headerRow->addStretch();
    headerRow->addWidget(m_info);
    outer->addWidget(header);

    m_body = new QVBoxLayout;
    m_body->setContentsMargins(6, 6, 6, 6);
    m_body->setSpacing(6);
    outer->addLayout(m_body, 1);
}

void RetroPanel::setHeaderInfo(const QString &text)
{
    m_info->setText(text);
}
