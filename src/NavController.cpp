#include "NavController.h"

#include <QDebug>

NavController::NavController(const QString &homePage, QObject *parent)
    : QObject(parent)
{
    m_stack.append(homePage);
}

void NavController::push(const QString &page)
{
    qInfo() << "NavController: push" << page << "(depth" << m_stack.size() << "->" << m_stack.size() + 1 << ")";
    m_stack.append(page);
    emit currentPageChanged();
}

void NavController::pop()
{
    if (m_stack.size() <= 1) {
        qInfo() << "NavController: pop ignored, already at root";
        return;
    }
    qInfo() << "NavController: pop" << m_stack.last() << "(depth" << m_stack.size() << "->" << m_stack.size() - 1 << ")";
    m_stack.removeLast();
    emit currentPageChanged();
}

void NavController::replace(const QString &page)
{
    qInfo() << "NavController: replace" << m_stack.last() << "->" << page << "(depth stays" << m_stack.size() << ")";
    if (!m_stack.isEmpty())
        m_stack.removeLast();
    m_stack.append(page);
    emit currentPageChanged();
}

QString NavController::currentPage() const
{
    return m_stack.last();
}
