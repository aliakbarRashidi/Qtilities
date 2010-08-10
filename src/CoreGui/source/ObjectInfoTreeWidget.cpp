/****************************************************************************
**
** Copyright (c) 2009-2010, Jaco Naude
**
** This file is part of Qtilities which is released under the following
** licensing options.
**
** Option 1: Open Source
** Under this license Qtilities is free software: you can
** redistribute it and/or modify it under the terms of the GNU General
** Public License as published by the Free Software Foundation, either
** version 3 of the License, or (at your option) any later version.
**
** Qtilities is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Qtilities. If not, see http://www.gnu.org/licenses/.
**
** Option 2: Commercial
** Alternatively, this library is also released under a commercial license
** that allows the development of closed source proprietary applications
** without restrictions on licensing. For more information on this option,
** please see the project website's licensing page:
** http://www.qtilities.org/licensing.html
**
** If you are unsure which license is appropriate for your use, please
** contact support@qtilities.org.
**
****************************************************************************/

#include "ObjectInfoTreeWidget.h"
#include "QtilitiesCoreGuiConstants.h"
#include "QtilitiesCoreGui.h"

#include <QtilitiesCoreConstants.h>
#include <ObserverProperty.h>
#include <Observer.h>

#include <QMetaObject>
#include <QMetaMethod>
#include <QEvent>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>

using namespace Qtilities::CoreGui::Constants;
using namespace Qtilities::CoreGui::Icons;
using namespace Qtilities::CoreGui::Actions;
using namespace Qtilities::Core::Properties;
using namespace Qtilities::Core;

QPointer<Qtilities::CoreGui::ObjectInfoTreeWidget> Qtilities::CoreGui::ObjectInfoTreeWidget::currentWidget;
QPointer<Qtilities::CoreGui::ObjectInfoTreeWidget> Qtilities::CoreGui::ObjectInfoTreeWidget::actionContainerWidget;

struct Qtilities::CoreGui::ObjectInfoTreeWidgetData {
    ObjectInfoTreeWidgetData() : paste_enabled(false) {}

    bool paste_enabled;
};

Qtilities::CoreGui::ObjectInfoTreeWidget::ObjectInfoTreeWidget(QWidget *parent) : QTreeWidget(parent) {
    d = new ObjectInfoTreeWidgetData;

    setColumnCount(1);
    setSortingEnabled(true);
    headerItem()->setText(0,tr("Registered Objects"));
    currentWidget = 0;
}

Qtilities::CoreGui::ObjectInfoTreeWidget::~ObjectInfoTreeWidget() {
    delete d;
}

void Qtilities::CoreGui::ObjectInfoTreeWidget::mousePressEvent(QMouseEvent* event) {
    if (!event)
        return;

    if (event->type() == QEvent::MouseButtonPress) {
        if (currentWidget != this) {
            // Disconnect the paste action from the previous observer.
            Command* command = QtilitiesCoreGui::instance()->actionManager()->command(MENU_EDIT_PASTE);
            if (command->action())
                command->action()->disconnect(currentWidget);

            // Connect to the paste action
            connect(command->action(),SIGNAL(triggered()),SLOT(handle_actionPaste_triggered()));
        }

        currentWidget = this;
    }

    QTreeWidget::mousePressEvent(event);
}

void Qtilities::CoreGui::ObjectInfoTreeWidget::setObjectMap(QMap<QPointer<QObject>, QString> object_map) {
   clear();

    // Build up a tree using the object map
    // Check all the categories by looking at the OBJECT_CATEGORY observer property on the objects.
    // Objects without a category property will not be shown
    QList<QTreeWidgetItem *> items;
    for (int i = 0; i < object_map.count(); i++) {
        if (!object_map.keys().at(i))
            break;

        QTreeWidgetItem* item = 0;
        QString category_string = tr("More...");
        QVariant prop = object_map.keys().at(i)->property(OBJECT_CATEGORY);
        if (prop.isValid() && prop.canConvert<SharedObserverProperty>()) {
            SharedObserverProperty observer_property =  prop.value<SharedObserverProperty>();
            if (observer_property.isValid()) {
                // Check if the top level category already exists
                category_string = observer_property.value().toString();
            }
        }

        for (int t = 0; t < topLevelItemCount(); t++) {
            if (topLevelItem(t)->text(0) == category_string)
                item = topLevelItem(t);
        }
        if (!item) {
            item = new QTreeWidgetItem((QTreeWidget*)0, QStringList(category_string));
            item->setData(0,Qt::DecorationRole,QIcon(QString(ICON_FOLDER_16X16)));
            items.append(item);
        }

        addTopLevelItem(item);

        // Add this object to the category item
        QTreeWidgetItem* child = new QTreeWidgetItem((QTreeWidget*)0, QStringList(object_map.values().at(i)));
        item->addChild(child);

        // Populate the child item
        populateItem(child,object_map.keys().at(i));

        // Check if it has the OBJECT_ICON shared property set.
        prop = object_map.keys().at(i)->property(OBJECT_ICON);
        if (prop.isValid() && prop.canConvert<SharedObserverProperty>())
            child->setIcon(0,(prop.value<SharedObserverProperty>().value().value<QIcon>()));

        // Handle the case where the object has an observer as a child
        /*foreach (QObject* child, item->getObject()->children()) {
            Observer* child_observer = qobject_cast<Observer*> (child);
            if (child_observer) {
                observer = child_observer;
            }
        }*/
    }
    expandToDepth(0);
    sortItems(0,Qt::AscendingOrder);
}

void Qtilities::CoreGui::ObjectInfoTreeWidget::setHierarchyDepth(int depth) {

}

void Qtilities::CoreGui::ObjectInfoTreeWidget::setPasteEnabled(bool enable_paste) {
    d->paste_enabled = enable_paste;
}

void Qtilities::CoreGui::ObjectInfoTreeWidget::constructActions() {
    if (actionContainerWidget)
        return;
    actionContainerWidget = this;
}

void Qtilities::CoreGui::ObjectInfoTreeWidget::refreshActions() {
    if (!actionContainerWidget)
        constructActions();

    if (!currentWidget)
        return;
}

void Qtilities::CoreGui::ObjectInfoTreeWidget::handle_actionPaste_triggered() {
    if (!currentWidget)
        return;

    if (currentWidget->d->paste_enabled){
        // Check if the subjects being dropped are of the same type as the destination observer.
        // If this is not the case, we do not allow the drop.
        const ObserverMimeData* observer_mime_data = qobject_cast<const ObserverMimeData*> (QApplication::clipboard()->mimeData());
        if (observer_mime_data) {
            emit currentWidget->pasteActionOccured(observer_mime_data);
        } else {
            QMessageBox msgBox;
            msgBox.setText("Paste Operation Failed.");
            msgBox.setInformativeText("The paste operation could not be completed. Unsupported data type for this context.\n\nDo you want to keep the data in the clipboard for later usage?");
            msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            msgBox.setDefaultButton(QMessageBox::No);
            int ret = msgBox.exec();

            switch (ret) {
              case QMessageBox::No:
                  QtilitiesCoreGui::instance()->clipboardManager()->acceptMimeData();
                  break;
              case QMessageBox::Yes:
                  break;
              default:
                  break;
            }
        }
    }
}

void Qtilities::CoreGui::ObjectInfoTreeWidget::populateItem(QTreeWidgetItem* item, QObject* obj) {
    // Add the following categories:
    // 1. Methods (Signals and Slots)
    // 2. Properties
    // 3. Events
    // Further categories can be added by subclassing this class.
    if (item) {
        // Methods:
        QTreeWidgetItem* methods = new QTreeWidgetItem((QTreeWidget*)0, QStringList(tr("Methods")));
        methods->setIcon(0, QIcon(ICON_METHOD));
        item->addChild(methods);
        QTreeWidgetItem* events = new QTreeWidgetItem((QTreeWidget*)0, QStringList(tr("Events")));
        events->setIcon(0, QIcon(ICON_EVENT));
        item->addChild(events);
        const QMetaObject* mo = obj->metaObject();
        for(int j=QObject::staticMetaObject.methodCount(); j<mo->methodCount(); j++)
        {
            QMetaMethod m = mo->method(j);
            QTreeWidgetItem* item =  0;

            switch(m.methodType())
            {
            case QMetaMethod::Signal: {
                item = new QTreeWidgetItem(events, Event);
                QString event = m.signature();
                item->setText(0, event);
                item->setData(0, Qt::ForegroundRole, Qt::blue);
                } break;
            case QMetaMethod::Method:
            case QMetaMethod::Slot:
            default: {
                if(m.access() != QMetaMethod::Public)
                    break;
                item = new QTreeWidgetItem(methods, Method);
                QString methodName = QString(m.signature()).section('(', 0, 0);
                item->setText(0, methodName);
                item->setData(0, Qt::ForegroundRole, Qt::darkGreen);
                } break;
            }
        }

        // Properties:
        QTreeWidgetItem* properties = new QTreeWidgetItem((QTreeWidget*)0, QStringList(tr("Properties")));
        properties->setIcon(0, QIcon(ICON_PROPERTY));
        item->addChild(properties);
        for(int j=QObject::staticMetaObject.propertyCount(); j<mo->propertyCount(); j++)
        {
            QMetaProperty prop = mo->property(j);
            QTreeWidgetItem* item = new QTreeWidgetItem(properties, Property);
            item->setData(0, Qt::ForegroundRole, Qt::red);
            item->setText(0, prop.name());
        }

        // Allow other classes to add categories to the item tree item
        emit populateTreeItem(obj, item);
    }
}