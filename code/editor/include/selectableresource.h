#ifndef SELECTABLERESOURCE_H
#define SELECTABLERESOURCE_H

#include <QDialog>
#include <functional>

namespace Ui { class SelectableResource; }

class SelectableResource : public QDialog
{
    Q_OBJECT

public:
    explicit SelectableResource(QWidget *parent = 0);
    ~SelectableResource();

    void AddResource(const QString& name);
    void DelResource(const QString& name);
    void SelectResource(std::function<void (QString)> callback);

#define ADD_SELECTABLE_RESOURCE(name) \
    static SelectableResource& name(void) \
    { \
      static SelectableResource* ptr = new SelectableResource; \
      \
      return (*ptr); \
    }

    ADD_SELECTABLE_RESOURCE(MapsResource)
    ADD_SELECTABLE_RESOURCE(Charsheets)
    ADD_SELECTABLE_RESOURCE(AIs)
    ADD_SELECTABLE_RESOURCE(Encounters)
    ADD_SELECTABLE_RESOURCE(Dialogs)
    ADD_SELECTABLE_RESOURCE(ObjectScript)

private slots:
    void SearchList(void);
    void SelectedItem(void);

private:
    Ui::SelectableResource *ui;

    QList<QString>                resources;
    std::function<void (QString)> callback;
};

#endif // SELECTABLERESOURCE_H