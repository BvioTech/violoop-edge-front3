#pragma once

#include <QObject>
#include <QStringList>

// Page navigation, take three. Both QML-native mechanisms tried first
// failed in ways invisible until real-device touch testing:
//   1. A hand-rolled `pragma Singleton` QtObject with a `function push()` -
//      callable in principle, but calling it threw "TypeError: Property
//      'push' of object Nav is not a function" at runtime, no build/load
//      warning beyond an easy-to-miss qmlcachegen diskcache line.
//   2. StackView's own documented `StackView.view` attached property -
//      read as `null` from inside the pushed item's own handlers, both
//      for `initialItem` AND for an item pushed explicitly from
//      Component.onCompleted ("Cannot read property 'push' of null").
// Rather than keep guessing at a third QML-side workaround, navigation is
// now a plain C++ QObject exposed as a context property ("nav"), same
// proven pattern as RestClient/StatusStore/etc. - no singleton
// registration, no attached properties, just a Q_PROPERTY and two
// Q_INVOKABLEs. AppShell.qml drives a `Loader` off `nav.currentPage`
// instead of a `StackView`.
class NavController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentPage READ currentPage NOTIFY currentPageChanged)
    Q_PROPERTY(int depth READ depth NOTIFY currentPageChanged)

public:
    explicit NavController(const QString &homePage, QObject *parent = nullptr);

    Q_INVOKABLE void push(const QString &page);
    Q_INVOKABLE void pop();

    QString currentPage() const;
    int depth() const { return m_stack.size(); }

signals:
    void currentPageChanged();

private:
    QStringList m_stack;
};
