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

#include "Observer.h"
#include "QtilitiesCore.h"
#include "QtilitiesCoreConstants.h"
#include "ObserverProperty.h"
#include "ActivityPolicyFilter.h"
#include "QtilitiesPropertyChangeEvent.h"
#include "ObserverMimeData.h"

#include <Logger.h>

#include <QMap>
#include <QVariant>
#include <QMetaType>
#include <QEvent>
#include <QDynamicPropertyChangeEvent>
#include <QCoreApplication>
#include <QMutableListIterator>
#include <QtDebug>

using namespace Qtilities::Core::Constants;
using namespace Qtilities::Core::Properties;

struct Qtilities::Core::ObserverHints {
public:
    ObserverHints() :
            naming_control(Observer::NoNamingControlHint),
            activity_display(Observer::NoActivityDisplayHint),
            activity_control(Observer::NoActivityControlHint),
            item_selection_control(Observer::SelectableItems),
            hierarhical_display(Observer::NoHierarhicalDisplayHint),
            display_flags(Observer::ItemView | Observer::NavigationBar),
            item_view_column_hint(Observer::NoItemViewColumnHint),
            action_hints(Observer::None),
            category_list(QStringList()),
            inverse_categories(true),
            category_filter_enabled(false) {}
    ObserverHints(const ObserverHints& other) {
        naming_control = other.naming_control;
        activity_display = other.activity_display;
        activity_control = other.activity_control;
        item_selection_control = other.item_selection_control;
        hierarhical_display = other.hierarhical_display;
        display_flags = other.display_flags;
        item_view_column_hint = other.item_view_column_hint;
        action_hints = other.action_hints;
        category_list = other.category_list;
        inverse_categories = other.inverse_categories;
        category_filter_enabled = other.category_filter_enabled;
    }
    void operator=(const ObserverHints& other) {
        naming_control = other.naming_control;
        activity_display = other.activity_display;
        activity_control = other.activity_control;
        item_selection_control = other.item_selection_control;
        hierarhical_display = other.hierarhical_display;
        display_flags = other.display_flags;
        item_view_column_hint = other.item_view_column_hint;
        action_hints = other.action_hints;
        category_list = other.category_list;
        inverse_categories = other.inverse_categories;
        category_filter_enabled = other.category_filter_enabled;
    }

    bool exportBinary(QDataStream& stream) const {
        stream << (quint32) naming_control;
        stream << (quint32) activity_display;
        stream << (quint32) activity_control;
        stream << (quint32) item_selection_control;
        stream << (quint32) hierarhical_display;
        stream << (quint32) display_flags;
        stream << (quint32) item_view_column_hint;
        stream << (quint32) action_hints;
        stream << category_list;
        stream << inverse_categories;
        stream << category_filter_enabled;
        return true;
    }
    bool importBinary(QDataStream& stream) {
        quint32 qi32;
        stream >> qi32;
        naming_control = Observer::NamingControl (qi32);
        stream >> qi32;
        activity_display = Observer::ActivityDisplay (qi32);
        stream >> qi32;
        activity_control = Observer::ActivityControl (qi32);
        stream >> qi32;
        item_selection_control = Observer::ItemSelectionControl (qi32);
        stream >> qi32;
        hierarhical_display = Observer::HierarhicalDisplay (qi32);
        stream >> qi32;
        display_flags = Observer::DisplayFlags (qi32);
        stream >> qi32;
        item_view_column_hint = Observer::ItemViewColumnFlags (qi32);
        stream >> qi32;
        action_hints = Observer::ActionHints (qi32);
        stream >> category_list;
        stream >> inverse_categories;
        stream >> category_filter_enabled;
        return true;
    }

    Observer::NamingControl         naming_control;
    Observer::ActivityDisplay       activity_display;
    Observer::ActivityControl       activity_control;
    Observer::ItemSelectionControl  item_selection_control;
    Observer::HierarhicalDisplay    hierarhical_display;
    Observer::DisplayFlags          display_flags;
    Observer::ItemViewColumnFlags   item_view_column_hint;
    Observer::ActionHints           action_hints;
    QStringList                     category_list;
    bool                            inverse_categories;
    bool                            category_filter_enabled;
};

Qtilities::Core::Observer::Observer(const QString& observer_name, const QString& observer_description, QObject* parent) : QObject(parent) {
    // Initialize observer data
    observerData = new ObserverData;
    setObjectName(observer_name);
    observerData->observer_description = observer_description;
    observerData->access_mode_scope = GlobalScope;
    observerData->access_mode = FullAccess;
    observerData->ignore_dynamic_property_changes = false;
    observerData->subject_limit = -1;
    observerData->subject_id_counter = 0;
    observerData->subject_list.setObjectName(QString("%1 Pointer List").arg(observer_name));
    connect(&observerData->subject_list,SIGNAL(objectDestroyed(QObject*)),SLOT(handle_deletedSubject(QObject*)));
    observerData->display_hints = new ObserverHints;

    // Register this observer with the observer manager
    if (observer_name != QString(GLOBAL_OBJECT_POOL))
        observerData->observer_id = QtilitiesCore::instance()->objectManager()->registerObserver(this);
    else
        observerData->observer_id = 0;

    // To catch name changes.
    installEventFilter(this);
}

Qtilities::Core::Observer::Observer(const Observer &other) : observerData (other.observerData) {
    connect(&observerData->subject_list,SIGNAL(objectDestroyed(QObject*)),SLOT(handle_deletedSubject(QObject*)));
}

Qtilities::Core::Observer::~Observer() {
    if (objectName() != QString(GLOBAL_OBJECT_POOL)) {
        startProcessingCycle();
        // When this observer is deleted, we must check the ownership of all its subjects
        QVariant subject_ownership_variant;
        QVariant parent_observer_variant;

        LOG_TRACE(QString("Starting destruction of observer \"%1\":").arg(objectName()));
        LOG_TRACE("Deleting neccessary children.");

        QMutableListIterator<QObject*> i(observerData->subject_list);
        while (i.hasNext()) {
            QObject* obj = i.next();
            subject_ownership_variant = getObserverPropertyValue(obj,OWNERSHIP);
            parent_observer_variant = getObserverPropertyValue(obj,OBSERVER_PARENT);
            if ((subject_ownership_variant.toInt() == SpecificObserverOwnership) && (observerData->observer_id == parent_observer_variant.toInt())) {
                // Subjects with SpecificObserverOwnership must be deleted as soon as this observer is deleted if this observer is their parent.
               LOG_TRACE(QString("Object \"%1\" (aliased as %2 in this context) is owned by this observer, it will be deleted.").arg(obj->objectName()).arg(subjectNameInContext(obj)));
               if (!i.hasNext()) {
                   delete obj;
                   break;
               }
               else
                   delete obj;
            } else if ((subject_ownership_variant.toInt() == ObserverScopeOwnership) && (parentCount(obj) == 1)) {
               LOG_TRACE(QString("Object \"%1\" (aliased as %2 in this context) with ObserverScopeOwnership went out of scope, it will be deleted.").arg(obj->objectName()).arg(subjectNameInContext(obj)));
               if (!i.hasNext()) {
                   delete obj;
                   break;
               }
               else
                   delete obj;
           } else if ((subject_ownership_variant.toInt() == OwnedBySubjectOwnership) && (parentCount(obj) == 1)) {
              LOG_TRACE(QString("Object \"%1\" (aliased as %2 in this context) with OwnedBySubjectOwnership went out of scope, it will be deleted.").arg(obj->objectName()).arg(subjectNameInContext(obj)));
              if (!i.hasNext()) {
                  delete obj;
                  break;
              }
              else
                  delete obj;
           }
        }
        observerData->process_cycle_active = false;
    }

    // Delete subject filters
    LOG_TRACE("Deleting subject filters.");
    observerData->subject_filters.deleteAll();

    if (objectName() != QString(GLOBAL_OBJECT_POOL)) {
        LOG_TRACE("Removing any trace of this observer from remaining children.");
        int count = subjectCount();
        for (int i = 0; i < count; i++) {
            // In this case we need to remove any trace of this observer from the obj
            removeObserverProperties(observerData->subject_list.at(i));
        }
    }

    if (observerData->display_hints)
        delete observerData->display_hints;

    LOG_DEBUG(QString("Done with destruction of observer \"%1\".").arg(objectName()));
}

QStringList Qtilities::Core::Observer::monitoredProperties() const {
    QStringList properties;
    properties << QString(OBSERVER_SUBJECT_IDS) << QString(OWNERSHIP) << QString(OBSERVER_PARENT) << QString(OBSERVER_VISITOR_ID) << QString(OBJECT_LIMITED_EXPORTS) << QString(OBJECT_ICON) << QString(OBJECT_TOOLTIP) << QString(OBJECT_CATEGORY) << QString(OBJECT_ACCESS_MODE);

    for (int i = 0; i < observerData->subject_filters.count(); i++) {
        properties << observerData->subject_filters.at(i)->monitoredProperties();
    }
    return properties;
}

void Qtilities::Core::Observer::disableSubjectEventFiltering() {
    observerData->ignore_dynamic_property_changes = true;
}

void Qtilities::Core::Observer::enableSubjectEventFiltering() {
    observerData->ignore_dynamic_property_changes = false;
}

bool Qtilities::Core::Observer::eventFilter(QObject *object, QEvent *event)
{
    if ((event->type() == QEvent::DynamicPropertyChange)) {
        // Get the even tin the correct format
        QDynamicPropertyChangeEvent* propertyChangeEvent = static_cast<QDynamicPropertyChangeEvent *>(event);

        // Handle dynamic property changes on subjects
        if (contains(object) && monitoredProperties().contains(QString(propertyChangeEvent->propertyName().data()))) {
            // Ok we checked that the property is managed by this observer or any of its subject filters

            // Ignore the property change, just pass it through without filtering it.
            // Property changes from within observer sets this to true before changing properties.
            if (observerData->ignore_dynamic_property_changes)
                return false;

            // Handle changes from different threads
            if (!observerData->observer_mutex.tryLock())
                return true;

            // We now route the event that changed to the subject filter responsible for this property to validate the change
            // If no subject filter is responsible, the observer needs to handle it itself.
            bool filter_event = false;
            bool is_filter_property = false;
            for (int i = 0; i < observerData->subject_filters.count(); i++) {
                if (observerData->subject_filters.at(i)) {
                    if (observerData->subject_filters.at(i)->monitoredProperties().contains(QString(propertyChangeEvent->propertyName().data()))) {
                        is_filter_property = true;
                        filter_event = observerData->subject_filters.at(i)->monitoredPropertyChanged(object, propertyChangeEvent->propertyName().data(),propertyChangeEvent);
                    }
                }
            }

            // Ok if is not a property managed by a subject filter, it is one of the observer's own properties.
            // In this case we must filter the ones that can't be modified.
            if (!is_filter_property) {
                if ((!strcmp(propertyChangeEvent->propertyName().data(),OBSERVER_SUBJECT_IDS)) ||
                   (!strcmp(propertyChangeEvent->propertyName().data(),OBSERVER_LIMIT)) ||
                   (!strcmp(propertyChangeEvent->propertyName().data(),OWNERSHIP)) ||
                   (!strcmp(propertyChangeEvent->propertyName().data(),OBSERVER_PARENT))) {
                    // Filter any changes to these properties. They are read-only.
                    filter_event  = true;
                } else {
                    filter_event = false;
                }
            }

            // If the event should not be filtered, we need to post a user event on the object which will indicate that the
            // property change was valid and succesfull.
            if (!filter_event) {
                if ((!strcmp(propertyChangeEvent->propertyName().data(),OBSERVER_VISITOR_ID)) ||
                   (!strcmp(propertyChangeEvent->propertyName().data(),OBJECT_LIMITED_EXPORTS))) {
                    // Filter any changes to these properties. They are read-only.
                    QByteArray property_name_byte_array = QByteArray(propertyChangeEvent->propertyName().data());
                    QtilitiesPropertyChangeEvent* user_event = new QtilitiesPropertyChangeEvent(property_name_byte_array,observerID());
                    QCoreApplication::postEvent(object,user_event);
                    emit propertyBecameDirty(propertyChangeEvent->propertyName(),object);
                    LOG_TRACE(QString("Posting QtilitiesPropertyChangeEvent (property: %1) to object (%2)").arg(QString(propertyChangeEvent->propertyName().data())).arg(object->objectName()));
                }
            }

            // Unlock the mutex and return
            observerData->observer_mutex.unlock();
            return filter_event;
        } else if (object == this) {
            // We need to check if the name of this observer changed. If it is observed by an observer with a
            // naming policy subject filter we just pass the event through since the naming policy subject filter will take care of it
            // and post a custom qtilities property change event. This event will be caugth by this event filter again.
            // If the object is not observed we sync objectName() with the new property and emit nameChanged()
            if (!strcmp(propertyChangeEvent->propertyName().data(),OBJECT_NAME)) {
                bool sync_name = true;
                /*ObserverProperty observer_list = getObserverProperty(this,OBSERVER_SUBJECT_IDS);
                if (observer_list.isValid()) {
                    Observer* tmp_observer;
                    for (int i = 0; i < observer_list.observerMap().count(); i++) {
                        tmp_observer = QtilitiesCore::instance()->objectManager()->observerReference(observer_list.observerMap().keys().at(i));
                        if (tmp_observer) {
                            for (int r = 0; r < tmp_observer->subjectFilters().count(); r++) {
                                NamingPolicyFilter* naming_filter = qobject_cast<NamingPolicyFilter*> (tmp_observer->subjectFilters().at(r));
                                if (naming_filter)
                                    sync_name = false;
                            }
                        }
                    }
                }*/

                if (sync_name) {
                    QString new_name = getObserverPropertyValue(this,OBJECT_NAME).toString();
                    setObjectName(new_name);
                    objectName() = new_name;
                    emit nameChanged(getObserverPropertyValue(this,OBJECT_NAME).toString());
                    setModificationState(true);
                }
            }
        }
    } else if ((event->type() == QEvent::User)) {
        // We check if the event is a QtilitiesPropertyChangeEvent.
        QtilitiesPropertyChangeEvent* qtilities_event = static_cast<QtilitiesPropertyChangeEvent *> (event);
        if (qtilities_event) {
            if (!strcmp(qtilities_event->propertyName().data(),OBJECT_NAME) && (object == this)) {
                // The object name will already be sync'ed at this stage.
                observerData->subject_list.setObjectName(QString("%1 Pointer List").arg(observerName(qtilities_event->observerID())));
                if (!observerData->process_cycle_active) {
                    //emit nameChanged(observerName(qtilities_event->observerID()));
                    //setModificationState(true);
                }
            }
        }
    }
    return false;
}

Qtilities::Core::Interfaces::IExportable::ExportModeFlags Qtilities::Core::Observer::supportedFormats() const {
    IExportable::ExportModeFlags flags = 0;
    flags |= IExportable::Binary;
    return flags;
}

quint32 MARKER_OBSERVER_SECTION = 0xBBBBBBBB;

Qtilities::Core::Interfaces::IExportable::Result Qtilities::Core::Observer::exportBinary(QDataStream& stream) const {
    LOG_TRACE(tr("Binary export of observer ") + observerName() + tr(" section started."));

    // We define a succesfull operation as an export which is able to export all subjects.
    bool success = true;
    bool complete = true;

    // First export the factory data of this observer:
    IFactoryData factory_data = factoryData();
    factory_data.exportBinary(stream);

    // Stream the observerData class, this DOES NOT include the subjects itself, only the subject count.
    // It also excludes the factory data which was stream above.
    // This is neccessary because we want to keep track of the return values for subject IExportable interfaces.
    stream << MARKER_OBSERVER_SECTION;
    stream << *(observerData.data());
    success = observerData->display_hints->exportBinary(stream);
    stream << MARKER_OBSERVER_SECTION;

    // Stream details about the subject filters in to be added to the observer.
    // ToDo: in the future when the user has the ability to add custom subject filters, or
    // change subject filter details.

    // Count the number of IExportable subjects first and check if objects must be excluded.
    QList<IExportable*> exportable_list;
    qint32 iface_count = 0;
    for (int i = 0; i < observerData->subject_list.count(); i++) {
        QObject* obj = observerData->subject_list.at(i);
        IExportable* iface = qobject_cast<IExportable*> (obj);
        if (!iface) {
            foreach (QObject* child, obj->children()) {
                Observer* obs = qobject_cast<Observer*> (child);
                if (obs) {
                    iface = obs;
                    break;
                }
            }
        }

        if (iface) {
            if (iface->supportedFormats() & IExportable::Binary) {
                // Handle limited export object, thus they should only be exported once.
                bool ok = false;
                int export_count = getObserverPropertyValue(obj,OBJECT_LIMITED_EXPORTS).toInt(&ok);
                if (export_count == 0) {
                    if (ok)
                        setObserverPropertyValue(obj,OBJECT_LIMITED_EXPORTS,export_count+1);

                    exportable_list << iface;
                    ++iface_count;
                } else {
                    LOG_TRACE(QString("%1/%2: Limited export object \"%3\" excluded in this context...").arg(i).arg(iface_count).arg(subjectNameInContext(obj)));
                }
            } else {
                LOG_WARNING(tr("Binary export found an interface (") + subjectNameInContext(obj) + tr(" in context ") + observerName() + tr(") in context which does not support binary exporting. Binary export will be incomplete."));
                complete = false;
            }
        } else {
            LOG_WARNING(tr("Binary export found an object (") + subjectNameInContext(obj) + tr(" in context ") + observerName() + tr(") which does not implement an exportable interface. Binary export will be incomplete."));
            complete = false;
        }
    }

    // Write this count to the stream:
    stream << iface_count;
    LOG_TRACE(QString(tr("%1 exportable subjects found under this observer's level of hierarhcy.")).arg(iface_count));

    // Now check all subjects for the IExportable interface.
    for (int i = 0; i < exportable_list.count(); i++) {
        IExportable* iface = exportable_list.at(i);
        QObject* obj = iface->objectBase();
        LOG_TRACE(QString("%1/%2: Exporting \"%3\"...").arg(i).arg(iface_count).arg(subjectNameInContext(obj)));
        IExportable::Result result = iface->exportBinary(stream);

        // Now export the needed properties about this subject
        if (result == IExportable::Complete) {
            QtilitiesCore::instance()->objectManager()->exportObjectProperties(obj,stream);
        } else if (result == IExportable::Incomplete) {
            QtilitiesCore::instance()->objectManager()->exportObjectProperties(obj,stream);
            complete = false;
        } else if (result == IExportable::Failed)
            success = false;
    }
    stream << MARKER_OBSERVER_SECTION;
    if (success) {
        if (complete) {
            LOG_DEBUG(tr("Binary export of observer ") + observerName() + QString(tr(" was successfull (complete).")));
            return IExportable::Complete;
        } else {
            LOG_DEBUG(tr("Binary export of observer ") + observerName() + QString(tr(" was successfull (incomplete).")));
            return IExportable::Incomplete;
        }
    } else {
        LOG_WARNING(tr("Binary export of observer ") + observerName() + tr(" failed."));
        return IExportable::Failed;
    }
}

Qtilities::Core::Interfaces::IExportable::Result Qtilities::Core::Observer::importBinary(QDataStream& stream, QList<QPointer<QObject> >& import_list) {
    LOG_TRACE(tr("Binary import of observer ") + observerName() + tr(" section started."));
    startProcessingCycle();

    // We define a succesfull operation as an import which is able to import all subjects.
    bool success = true;
    bool complete = true;

    quint32 ui32;
    stream >> ui32;
    if (ui32 != MARKER_OBSERVER_SECTION) {
        LOG_ERROR("Observer binary import failed to detect marker located after factory data. Import will fail.");
        return IExportable::Failed;
    }
    Q_ASSERT(ui32 == MARKER_OBSERVER_SECTION);

    // Stream the observerData class, this DOES NOT include the subjects itself, only the subject count.
    // This is neccessary because we want to keep track of the return values for subject IExportable interfaces.
    stream >> *(observerData.data());
    success = observerData->display_hints->importBinary(stream);
    stream >> ui32;
    if (ui32 != MARKER_OBSERVER_SECTION) {
        LOG_ERROR("Observer binary import failed to detect marker located after observer data. Import will fail.");
        return IExportable::Failed;
    }

    // Stream details about the subject filters to be added to the observer.
    // ToDo: in the future when the user has the ability to add custom subject filters, or
    // change subject filter details.

    // Count the number of IExportable subjects first.
    qint32 iface_count = 0;
    stream >> iface_count;
    LOG_TRACE(QString(tr("%1 exportable subject(s) found under this observer's level of hierarhcy.")).arg(iface_count));

    // Now check all subjects for the IExportable interface.
    for (int i = 0; i < iface_count; i++) {
        if (!success)
            break;

        IFactoryData factoryData;
        if (!factoryData.importBinary(stream))
            return IExportable::Failed;
        if (!factoryData.isValid()) {
            LOG_TRACE(QString(tr("%1/%2: Importing a standard observer...")).arg(i+1).arg(iface_count));
            Observer* new_obs = new Observer(factoryData.d_instance_name,QString());
            new_obs->setObjectName(factoryData.d_instance_name);
            success = attachSubject(new_obs,Observer::ManualOwnership);
            import_list.append(new_obs);
            IExportable::Result result = new_obs->importBinary(stream, import_list);
            if (result == IExportable::Complete) {
                QtilitiesCore::instance()->objectManager()->importObjectProperties(new_obs,stream);
            } else if (result == IExportable::Incomplete) {
                QtilitiesCore::instance()->objectManager()->importObjectProperties(new_obs,stream);
                complete = false;
            } else if (result == IExportable::Failed) {
                success = false;
            }
        } else {
            LOG_TRACE(QString(tr("%1/%2: Importing subject type \"%3\" in factory \"%4\"...")).arg(i+1).arg(iface_count).arg(factoryData.d_instance_tag).arg(factoryData.d_factory_tag));

            IFactory* ifactory = QtilitiesCore::instance()->objectManager()->factoryReference(factoryData.d_factory_tag);
            if (ifactory) {
                factoryData.d_instance_context = observerData->observer_id;
                QObject* new_instance = ifactory->createInstance(factoryData);
                new_instance->setObjectName(factoryData.d_instance_name);
                if (new_instance) {
                    import_list.append(new_instance);
                    IExportable* exp_iface = qobject_cast<IExportable*> (new_instance);
                    if (exp_iface) {
                        IExportable::Result result = exp_iface->importBinary(stream, import_list);
                        if (result == IExportable::Complete) {
                            QtilitiesCore::instance()->objectManager()->importObjectProperties(new_instance,stream);
                            success = attachSubject(new_instance,Observer::ObserverScopeOwnership);
                        } else if (result == IExportable::Incomplete) {
                            QtilitiesCore::instance()->objectManager()->importObjectProperties(new_instance,stream);
                            success = attachSubject(new_instance,Observer::ObserverScopeOwnership);
                            complete = false;
                        } else if (result == IExportable::Failed) {
                            success = false;
                        }
                    } else {
                        // Handle deletion of import_list;
                        success = false;
                        break;
                    }
                } else {
                    // Handle deletion of import_list;
                    success = false;
                    break;
                }
            } else {
                return IExportable::Failed;
            }
        }
    }
    stream >> ui32;
    if (ui32 != MARKER_OBSERVER_SECTION) {
        LOG_ERROR("Observer binary import failed to detect end marker. Import will fail.");
        return IExportable::Failed;
    }

    if (success) {
        LOG_DEBUG(tr("Binary import of observer ") + observerName() + tr(" section was successfull."));
        endProcessingCycle();
        return IExportable::Complete;
    } else {
        LOG_WARNING(tr("Binary import of observer ") + observerName() + tr(" section failed."));
        endProcessingCycle();
        return IExportable::Failed;
    }
}

bool Qtilities::Core::Observer::isModified() const {
    if (observerData->is_modified)
        return true;

    for (int i = 0; i < observerData->subject_list.count(); i++) {
        IModificationNotifier* mod_iface = qobject_cast<IModificationNotifier*> (observerData->subject_list.at(i));
        if (mod_iface) {
            if (mod_iface->isModified())
                return true;
        }
    }
    return false;
}

void Qtilities::Core::Observer::setModificationState(bool new_state, bool notify_listeners, bool notify_subjects) {
    observerData->is_modified = new_state;
    /*QString sender_name = "No Sender";
    if (sender()) {
        sender_name = sender()->objectName();
        if (new_state) {
            LOG_WARNING(QString("Observer: %1, New State: True, Sender: %2").arg(observerName()).arg(sender_name));
            if (sender_name == "DFE1_PROC_NULL" && observerName() == "OPEN DESIGNS")
                int i = 5;
        } else
            LOG_WARNING(QString("Observer: %1, New State: False, Sender: %2").arg(observerName()).arg(sender_name));
    }*/
    if (notify_listeners && !observerData->process_cycle_active) {
        emit modificationStateChanged(new_state);
    }
    if (notify_subjects) {
        for (int i = 0; i < observerData->subject_list.count(); i++) {
            IModificationNotifier* mod_iface = qobject_cast<IModificationNotifier*> (observerData->subject_list.at(i));
            if (mod_iface) {
                mod_iface->setModificationState(new_state,false,true);
            }
        }
    }
}

void Qtilities::Core::Observer::refreshViews() const {
    emit partialStateChanged("Hierarchy");
}

void Qtilities::Core::Observer::startProcessingCycle() {
    if (observerData->process_cycle_active)
        return;

    observerData->process_cycle_active = true;
}

void Qtilities::Core::Observer::endProcessingCycle() {
    if (!observerData->process_cycle_active)
        return;

    observerData->process_cycle_active = false;
    refreshViews();
}

void Qtilities::Core::Observer::setFactoryData(IFactoryData factory_data) {
    if (factory_data.isValid())
        observerData->factory_data = factory_data;
}

Qtilities::Core::Interfaces::IFactoryData Qtilities::Core::Observer::factoryData() const {
    observerData->factory_data.d_instance_name = observerName();
    return observerData->factory_data;
}

bool Qtilities::Core::Observer::attachSubject(QObject* obj, Observer::ObjectOwnership object_ownership, bool broadcast) {
    #ifndef QT_NO_DEBUG
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return false;
    #endif
    
    if (canAttach(obj,object_ownership) == Rejected)
        return false;

    // Pass new object through all installed subject filters
    bool passed_filters = true;
    for (int i = 0; i < observerData->subject_filters.count(); i++) {
        bool result = observerData->subject_filters.at(i)->initializeAttachment(obj);
        if (passed_filters)
            passed_filters = result;
    }

    if (!passed_filters) {
        LOG_DEBUG(QString("Observer (%1): Object (%2) attachment failed, attachment was rejected by one or more subject filter.").arg(objectName()).arg(obj->objectName()));
        for (int i = 0; i < observerData->subject_filters.count(); i++) {
            observerData->subject_filters.at(i)->finalizeAttachment(obj,false);
        }
        removeObserverProperties(obj);
        return false;
    }

    // Details of the global object pool observer is not added to any objects.
    if (objectName() != QString(GLOBAL_OBJECT_POOL)) {
        // Now, add observer details to needed properties
        // Add observer details to property: OBSERVER_SUBJECT_IDS
        ObserverProperty subject_id_property = getObserverProperty(obj,OBSERVER_SUBJECT_IDS);
        if (subject_id_property.isValid()) {
            // Thus, the property already exists
            subject_id_property.addContext(QVariant(observerData->subject_id_counter),observerData->observer_id);
            setObserverProperty(obj,subject_id_property);
        } else {
            // We need to create the property and add it to the object
            ObserverProperty new_subject_id_property(OBSERVER_SUBJECT_IDS);
            new_subject_id_property.setIsExportable(false);
            new_subject_id_property.addContext(QVariant(observerData->subject_id_counter),observerData->observer_id);
            setObserverProperty(obj,new_subject_id_property);
        }
        observerData->subject_id_counter += 1;

        // Now that the object has the properties needed, we add it
        observerData->subject_list.append(obj);

        // Handle object ownership
        #ifndef QT_NO_DEBUG
            QString management_policy_string;
        #endif
        // Check if the object is already managed, and if so with what ownership flag it was attached to those observers.
        if (parentCount(obj) > 1) {
            QVariant current_ownership = getObserverPropertyValue(obj,OWNERSHIP);
            if (current_ownership.toInt() != OwnedBySubjectOwnership) {
                if (object_ownership == ObserverScopeOwnership) {
                    // Update the ownership to ObserverScopeOwnership
                    setObserverPropertyValue(obj,OWNERSHIP,QVariant(ObserverScopeOwnership));
                    setObserverPropertyValue(obj,OBSERVER_PARENT,QVariant(-1));
                    #ifndef QT_NO_DEBUG
                        management_policy_string = "Observer Scope Ownership";
                    #endif
                } else if (object_ownership == SpecificObserverOwnership) {
                    // Update the ownership to SpecificObserverOwnership
                    setObserverPropertyValue(obj,OWNERSHIP,QVariant(SpecificObserverOwnership));
                    setObserverPropertyValue(obj,OBSERVER_PARENT,QVariant(observerID()));
                    #ifndef QT_NO_DEBUG
                        management_policy_string = "Specific Observer Ownership";
                    #endif
                } else if (object_ownership == ManualOwnership) {
                    #ifndef QT_NO_DEBUG
                        if (current_ownership.toInt() == SpecificObserverOwnership) {
                            // Leave it as it is, it will already be SpecificObserverOwnership.
                            management_policy_string = "Manual Ownership (current: Specific Observer Ownership)";
                        } else if (current_ownership.toInt() == ObserverScopeOwnership) {
                            // Leave it as it is, it will already be ObserverScopeOwnership.
                            management_policy_string = "Manual Ownership (current: Observer Scope Ownership)";
                        } else {
                            // Leave it as it is, it will already be manual ownership.
                            if (obj->parent())
                                management_policy_string = "Manual Ownership (current: Manual Ownership with parent())";
                            else
                                management_policy_string = "Manual Ownership (current: Manual Ownership without parent())";
                        }
                    #endif
                } else if (object_ownership == AutoOwnership) {
                    if (current_ownership.toInt() == SpecificObserverOwnership) {
                          // Leave it as it is, it will already be SpecificObserverOwnership.
                        #ifndef QT_NO_DEBUG
                            management_policy_string = "Auto Ownership (kept Specific Observer Ownership)";
                        #endif
                    } else if (current_ownership.toInt() == ObserverScopeOwnership) {
                        // Leave it as it is, it will already be ObserverScopeOwnership.
                        #ifndef QT_NO_DEBUG
                            management_policy_string = "Auto Ownership (kept Observer Scope Ownership)";
                        #endif
                    } else {
                        // Check if the object already has a parent, otherwise we handle it as ObserverScopeOwnership.
                        if (!obj->parent()) {
                            setObserverPropertyValue(obj,OWNERSHIP,QVariant(ObserverScopeOwnership));
                            #ifndef QT_NO_DEBUG
                                management_policy_string = "Auto Ownership (had no parent, using Observer Scope Ownership)";
                            #endif
                        } else {
                            setObserverPropertyValue(obj,OWNERSHIP,QVariant(ManualOwnership));
                            #ifndef QT_NO_DEBUG
                                management_policy_string = "Auto Ownership (had parent, leave as Manual Ownership)";
                            #endif
                        }
                        setObserverPropertyValue(obj,OBSERVER_PARENT,QVariant(-1));
                    }
                } else if (object_ownership == OwnedBySubjectOwnership) {
                    connect(obj,SIGNAL(destroyed()),SLOT(deleteLater()));
                    #ifndef QT_NO_DEBUG
                        management_policy_string = "Owned By Subject Ownership (this context is now dependant on this subject, but the original ownership of the subject was not changed)";
                    #endif
                }
            } else {
                // When OwnedBySubjectOwnership, the new ownership is ignored. Thus when a subject was attached to a
                // context using OwnedBySubjectOwnership it is attached to all other contexts after that using OwnedBySubjectOwnership
                // as well.
                // This observer must be deleted as soon as this subject is deleted.
                connect(obj,SIGNAL(destroyed()),SLOT(deleteLater()));
                #ifndef QT_NO_DEBUG
                    management_policy_string = "Owned By Subject Ownership (was already observed using this ownership type).";
                #endif
            }
        } else {
            SharedObserverProperty ownership_property(QVariant(object_ownership),OWNERSHIP);
            ownership_property.setIsExportable(false);
            setSharedProperty(obj,ownership_property);
            SharedObserverProperty observer_parent_property(QVariant(-1),OBSERVER_PARENT);
            observer_parent_property.setIsExportable(false);
            setSharedProperty(obj,observer_parent_property);
            if (object_ownership == ManualOwnership) {
                // We don't care about this object's lifetime, its up to the user to manage the lifetime of this object.
                #ifndef QT_NO_DEBUG
                    management_policy_string = "Manual Ownership";
                #endif
            } else if (object_ownership == AutoOwnership) {
                // Check if the object already has a parent, otherwise we handle it as ObserverScopeOwnership.
                if (!obj->parent()) {
                    setObserverPropertyValue(obj,OWNERSHIP,QVariant(ObserverScopeOwnership));
                    #ifndef QT_NO_DEBUG
                        management_policy_string = "Auto Ownership (had no parent, using Observer Scope Ownership)";
                    #endif
                } else {
                    setObserverPropertyValue(obj,OWNERSHIP,QVariant(ManualOwnership));
                    #ifndef QT_NO_DEBUG
                        management_policy_string = "Auto Ownership (had parent, leave as Manual Ownership)";
                    #endif
                }
            } else if (object_ownership == SpecificObserverOwnership) {
                // This observer must be its parent.
                setObserverPropertyValue(obj,OWNERSHIP,QVariant(SpecificObserverOwnership));
                setObserverPropertyValue(obj,OBSERVER_PARENT,QVariant(observerID()));
                #ifndef QT_NO_DEBUG
                    management_policy_string = "Specific Observer Ownership";
                #endif
            } else if (object_ownership == ObserverScopeOwnership) {
                // This object must be deleted as soon as its not observed by any observers any more.
                //obj->setParent(0);
                #ifndef QT_NO_DEBUG
                    management_policy_string = "Observer Scope Ownership";
                #endif
            } else if (object_ownership == OwnedBySubjectOwnership) {
                // This observer must be deleted as soon as this subject is deleted.
                connect(obj,SIGNAL(destroyed()),SLOT(deleteLater()));
                #ifndef QT_NO_DEBUG
                    management_policy_string = "Owned By Subject Ownership";
                #endif
            }
        }

        // Install the observer as an eventFilter on the subject.
        // We do this last, otherwise all dynamic property changes will go through this event filter.
        obj->installEventFilter(this);

        // Check if the new subject implements the IModificationNotifier interface. If so we connect
        // to the modification changed signal:
        bool has_mod_iface = false;
        IModificationNotifier* mod_iface = qobject_cast<IModificationNotifier*> (obj);
        if (mod_iface) {
            connect(mod_iface->objectBase(),SIGNAL(modificationStateChanged(bool)),SLOT(setModificationState(bool)));
            connect(mod_iface->objectBase(),SIGNAL(partialStateChanged(QString)),SIGNAL(partialStateChanged(QString)));
            has_mod_iface = true;
        }

        // Emit neccesarry signals
        QList<QObject*> objects;
        objects << obj;
        if (broadcast && !observerData->process_cycle_active) {
            emit numberOfSubjectsChanged(Observer::SubjectAdded, objects);
            emit partialStateChanged("Hierarchy");
            setModificationState(true);
        }

        #ifndef QT_NO_DEBUG
            if (has_mod_iface)
                LOG_DEBUG(QString("Observer (%1): Now observing object \"%2\" with management policy: %3. This object's modification state is now monitored by this observer.").arg(objectName()).arg(obj->objectName()).arg(management_policy_string));
            else
                LOG_DEBUG(QString("Observer (%1): Now observing object \"%2\" with management policy: %3.").arg(objectName()).arg(obj->objectName()).arg(management_policy_string));
        #endif

        // Register object in global object pool
        QtilitiesCore::instance()->objectManager()->registerObject(obj);
    } else {
        // If it is the global object manager it will get here.
        observerData->subject_list.append(obj);

        // Install the observer as an eventFilter on the subject.
        // We do this last, otherwise all dynamic property changes will go through this event filter.
        obj->installEventFilter(this);

        // The global object pool does not care about modification states of subjects.

        // Emit neccesarry signals
        QList<QObject*> objects;
        objects << obj;
        if (broadcast && !observerData->process_cycle_active) {
            emit numberOfSubjectsChanged(Observer::SubjectAdded, objects);
            emit partialStateChanged("Hierarchy");
            setModificationState(true);
        }

        LOG_TRACE(QString("Object \"%1\" is now visible in the global object pool.").arg(obj->objectName()));
    }

    // Finalize the attachment in all subject filters, indicating that the attachment was succesfull.
    for (int i = 0; i < observerData->subject_filters.count(); i++) {
        observerData->subject_filters.at(i)->finalizeAttachment(obj,true);
    }

    return true;
}

bool Qtilities::Core::Observer::attachSubjects(QList<QObject*> objects, Observer::ObjectOwnership ownership, bool broadcast) {
    bool success = true;
    for (int i = 0; i < objects.count(); i++) {
        if (i == objects.count()-1) {
            if (!attachSubject(objects.at(i), ownership, broadcast) && success)
                success = false;
        } else {
            if (!attachSubject(objects.at(i), ownership, false) && success)
                success = false;
        }
    }
    if (success && !observerData->process_cycle_active) {
        emit partialStateChanged("Hierarchy");
        setModificationState(true);
    }
    return success;
}

bool Qtilities::Core::Observer::attachSubjects(ObserverMimeData* mime_data_object, Observer::ObjectOwnership ownership, bool broadcast) {
    bool success = true;
    for (int i = 0; i < mime_data_object->subjectList().count(); i++) {
        if (i == mime_data_object->subjectList().count()-1) {
            if (!attachSubject(mime_data_object->subjectList().at(i), ownership, broadcast) && success)
                success = false;
        } else {
            if (!attachSubject(mime_data_object->subjectList().at(i), ownership, false) && success)
                success = false;
        }
    }
    if (success && !observerData->process_cycle_active) {
        emit partialStateChanged("Hierarchy");
        setModificationState(true);
    }
    return success;
}

Qtilities::Core::Observer::EvaluationResult Qtilities::Core::Observer::canAttach(QObject* obj, Observer::ObjectOwnership) const {
    if (!obj)
        return Observer::Rejected;

    QVariant category_variant = this->getObserverPropertyValue(obj,OBJECT_CATEGORY);
    QString category_string = category_variant.toString();
    if (isConst(category_string)) {
        LOG_WARNING(QString(tr("Attaching object \"%1\" to observer \"%2\" is not allowed. This observer is const for the recieved object.")).arg(obj->objectName()).arg(objectName()));
        return Observer::Rejected;
    }

    // Check for circular dependancies
    const Observer* observer_cast = qobject_cast<Observer*> (obj);
    if (observer_cast) {
        if (isParentInHierarchy(observer_cast,this)) {
            LOG_WARNING(QString(tr("Attaching observer \"%1\" to observer \"%2\" will result in a circular dependancy.")).arg(obj->objectName()).arg(objectName()));
            return Observer::Rejected;
        }
    }

    // First evaluate new subject from Observer side
    if (observerData->subject_limit == observerData->subject_list.count()) {
        LOG_WARNING(QString(tr("Observer (%1): Object (%2) attachment failed, subject limit reached.")).arg(objectName()).arg(obj->objectName()));
        return Observer::Rejected;
    }


    if (objectName() != QString(GLOBAL_OBJECT_POOL)) {
        // Check if this subject is already monitored by this observer, if so abort.
        // This will ensure that no subject filters need to check for this, thus subject filters can assume that new attachments are actually new.
        ObserverProperty observer_list = getObserverProperty(obj,OBSERVER_SUBJECT_IDS);
        if (observer_list.isValid()) {
            if (observer_list.hasContext(observerData->observer_id)) {
                LOG_WARNING(QString(tr("Observer (%1): Object (%2) attachment failed, object is already observed by this observer.")).arg(objectName()).arg(obj->objectName()));
                return Observer::Rejected;
            }
        }

        // Evaluate dynamic properties on the object
        bool has_limit = false;
        int observer_limit;
        int observer_count = parentCount(obj);

        QVariant observer_limit_variant = getObserverPropertyValue(obj,OBSERVER_LIMIT);
        if (observer_limit_variant.isValid()) {
            observer_limit = observer_limit_variant.toInt(&has_limit);
        }

        if (has_limit) {
            if (observer_count == -1) {
                observer_count = 0;
                // No count yet, check if the limit is > 0
                if ((observer_limit < 1) && (observer_limit != -1)){
                    LOG_WARNING(QString(tr("Observer (%1): Object (%2) attachment failed, observer limit (%3) reached.")).arg(objectName()).arg(obj->objectName()).arg(observer_limit));
                    return Observer::Rejected;
                }
            } else {
                if (observer_count >= observer_limit) {
                    LOG_WARNING(QString(tr("Observer (%1): Object (%2) attachment failed, observer limit (%3) reached.")).arg(objectName()).arg(obj->objectName()).arg(observer_limit));
                    return Observer::Rejected;
                }
            }
        }
    }

    // Evaluate attachment in all installed subject filters
    bool was_rejected = false;
    bool was_conditional = false;
    AbstractSubjectFilter::EvaluationResult current_filter_evaluation;
    for (int i = 0; i < observerData->subject_filters.count(); i++) {
        current_filter_evaluation = observerData->subject_filters.at(i)->evaluateAttachment(obj);
        if (current_filter_evaluation == AbstractSubjectFilter::Rejected)
            was_rejected = true;
        if (current_filter_evaluation == AbstractSubjectFilter::Conditional)
            was_conditional = true;
    }

    if (was_rejected)
        return Observer::Rejected;

    if (was_conditional)
        return Observer::Conditional;

    return Observer::Allowed;
}

Qtilities::Core::Observer::EvaluationResult Qtilities::Core::Observer::canAttach(ObserverMimeData* mime_data_object) const {
    if (!mime_data_object)
        return Observer::Rejected;

    bool success = true;
    int not_allowed_count = 0;
    for (int i = 0; i < mime_data_object->subjectList().count(); i++) {
        if (canAttach(mime_data_object->subjectList().at(i)) == Observer::Rejected) {
            success = false;
            ++not_allowed_count;
        }
    }

    if (success)
        return Observer::Allowed;
    else {
        if (not_allowed_count != mime_data_object->subjectList().count())
            return Observer::Conditional;
        else
            return Observer::Rejected;
    }
}

void Qtilities::Core::Observer::handle_deletedSubject(QObject* obj) {
    #ifndef QT_NO_DEBUG
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return;
    #endif

    // Pass object through all installed subject filters
    bool passed_filters = true;
    for (int i = 0; i < observerData->subject_filters.count(); i++) {
        passed_filters = observerData->subject_filters.at(i)->initializeDetachment(obj,true);
    }

    if (!passed_filters) {
        LOG_ERROR(QString(tr("Observer (%1): Warning: Subject filter rejected detachment of deleted object (%2).")).arg(objectName()).arg(obj->objectName()));
    }

    for (int i = 0; i < observerData->subject_filters.count(); i++) {
        observerData->subject_filters.at(i)->finalizeDetachment(obj,passed_filters,true);
    }

    LOG_DEBUG(QString("Observer (%1) detected deletion of object (%2), updated observer context accordingly.").arg(objectName()).arg(obj->objectName()));

    // Emit neccesarry signals
    QList<QObject*> objects;
    objects << obj;
    if (!observerData->process_cycle_active) {
        emit numberOfSubjectsChanged(SubjectRemoved, objects);
        emit partialStateChanged("Hierarchy");
        setModificationState(true);
    }
}

bool Qtilities::Core::Observer::detachSubject(QObject* obj, bool broadcast) {
    #ifndef QT_NO_DEBUG
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return false;
    #endif

    if (canDetach(obj) == Rejected)
        return false;

    observerData->ignore_dynamic_property_changes = true;

    // Pass object through all installed subject filters
    bool passed_filters = true;
    for (int i = 0; i < observerData->subject_filters.count(); i++) {
        passed_filters = observerData->subject_filters.at(i)->initializeDetachment(obj);
        if (!passed_filters)
            break;
    }

    if (!passed_filters) {
        LOG_WARNING(QString(tr("Observer (%1): Object (%2) detachment failed, detachment was rejected by one or more subject filter.")).arg(objectName()).arg(obj->objectName()));
        for (int i = 0; i < observerData->subject_filters.count(); i++) {
            observerData->subject_filters.at(i)->finalizeDetachment(obj,false);
        }
        return false;
    } else {
        for (int i = 0; i < observerData->subject_filters.count(); i++) {
            observerData->subject_filters.at(i)->finalizeDetachment(obj,true);
        }
    }

    if (objectName() != QString(GLOBAL_OBJECT_POOL)) {
        #ifndef QT_NO_DEBUG
            QString debug_name = obj->objectName();
        #endif

        // Check the ownership property of this object
        QVariant ownership_variant = getObserverPropertyValue(obj,OWNERSHIP);
        if (ownership_variant.isValid() && (ownership_variant.toInt() == ObserverScopeOwnership)) {
            if ((parentCount(obj) == 1) && obj) {
                LOG_DEBUG(QString("Object (%1) went out of scope, it will be deleted.").arg(obj->objectName()));
                delete obj;
                obj = 0;
            } else {
                removeObserverProperties(obj);
                observerData->subject_list.removeOne(obj);
            }
        } else if (ownership_variant.isValid() && (ownership_variant.toInt() == SpecificObserverOwnership)) {
            QVariant observer_parent = getObserverPropertyValue(obj,OBSERVER_PARENT);
            if (observer_parent.isValid() && (observer_parent.toInt() == observerID()) && obj) {
                delete obj;
                obj = 0;
            } else {
                removeObserverProperties(obj);
                observerData->subject_list.removeOne(obj);
            }
        } else {
            removeObserverProperties(obj);
            observerData->subject_list.removeOne(obj);
        }

        #ifndef QT_NO_DEBUG
             LOG_DEBUG(QString("Observer (%1): Not observing object (%2) anymore.").arg(objectName()).arg(debug_name));
        #endif
    }

    // Disconnect all signals in this object:
    if (obj)
        obj->disconnect(this);

    // Broadcast if neccesarry
    if (broadcast && !observerData->process_cycle_active) {
        QList<QObject*> objects;
        objects << obj;
        emit numberOfSubjectsChanged(SubjectRemoved, objects);
        emit partialStateChanged("Hierarchy");
        setModificationState(true);
    }

    observerData->ignore_dynamic_property_changes = false;
    return true;
}

bool Qtilities::Core::Observer::detachSubjects(QList<QObject*> objects, bool broadcast) {
    bool success = true;

    QList<QObject*> success_list;
    for (int i = 0; i < objects.count(); i++) {
        if (!detachSubject(objects.at(i), false) && success)
            success = false;
        else
            success_list << objects.at(i);
    }

    // Broadcast if neccesarry
    if (broadcast && !observerData->process_cycle_active) {
        emit numberOfSubjectsChanged(SubjectRemoved, success_list);
        emit partialStateChanged("Hierarchy");
        setModificationState(true);
    }

    return success;
}

Qtilities::Core::Observer::EvaluationResult Qtilities::Core::Observer::canDetach(QObject* obj) const {
    if (objectName() != QString(GLOBAL_OBJECT_POOL)) {
        // Check if this subject is observed by this observer. If its not observed by this observer, we can't detach it.
        ObserverProperty observer_list_variant = getObserverProperty(obj,OBSERVER_SUBJECT_IDS);
        if (observer_list_variant.isValid()) {
            if (!observer_list_variant.hasContext(observerData->observer_id)) {
                LOG_DEBUG(QString("Observer (%1): Object (%2) detachment is not allowed, object is not observed by this observer.").arg(observerData->observer_id).arg(obj->objectName()));
                return Observer::Rejected;
            }
        } else {
            // This subject is not observed by anything, or obj points to a deleted object, thus just quit.
            return Observer::Rejected;
        }

        // Validate operation against access mode
        QVariant category_variant = this->getObserverPropertyValue(obj,OBJECT_CATEGORY);
        QString category_string = category_variant.toString();
        if (isConst(category_string)) {
            LOG_DEBUG(QString("Detaching object \"%1\" from observer \"%2\" is not allowed. This observer is const for the recieved object.").arg(obj->objectName()).arg(objectName()));
            return Observer::Rejected;
        }

        // Check the ownership property of this object
        QVariant ownership_variant = getObserverPropertyValue(obj,OWNERSHIP);
        if (ownership_variant.isValid() && (ownership_variant.toInt() == ObserverScopeOwnership)) {
            if (parentCount(obj) == 1)
                return Observer::LastScopedObserver;
        } else if (ownership_variant.isValid() && (ownership_variant.toInt() == SpecificObserverOwnership)) {
            QVariant observer_parent = getObserverPropertyValue(obj,OBSERVER_PARENT);
            if (observer_parent.isValid() && (observer_parent.toInt() == observerID()) && obj) {
                return Observer::IsParentObserver;
            }
        } else if (ownership_variant.isValid() && (ownership_variant.toInt() == OwnedBySubjectOwnership)) {
            LOG_DEBUG(QString("Detaching object \"%1\" from observer \"%2\" is not allowed. This observer is dependant on this subject. To remove the subject permanently, delete it.").arg(obj->objectName()).arg(objectName()));
            return Observer::Rejected;
        }
    }

    // Evaluate detachment in all installed subject filters
    bool was_rejected = false;
    bool was_conditional = false;
    AbstractSubjectFilter::EvaluationResult current_filter_evaluation;
    for (int i = 0; i < observerData->subject_filters.count(); i++) {
        current_filter_evaluation = observerData->subject_filters.at(i)->evaluateDetachment(obj);
        if (current_filter_evaluation == AbstractSubjectFilter::Rejected)
            was_rejected = true;
        if (current_filter_evaluation == AbstractSubjectFilter::Conditional)
            was_conditional = true;
    }

    if (was_rejected)
        return Observer::Rejected;

    if (was_conditional)
        return Observer::Conditional;

    return Observer::Allowed;
}

void Qtilities::Core::Observer::detachAll() {
    int total = observerData->subject_list.count();
    startProcessingCycle();
    QList<QObject*> objects;
    objects << subjectReferences();
    for (int i = 0; i < total; i++) {
        detachSubject(observerData->subject_list.at(0), false);
    }
    // Make sure everything is gone
    observerData->subject_list.clear();
    observerData->process_cycle_active = false;
    emit numberOfSubjectsChanged(SubjectRemoved, objects);
    emit partialStateChanged("Hierarchy");
    setModificationState(true);
}

void Qtilities::Core::Observer::deleteAll() {
    int total = observerData->subject_list.count();
    if (total == 0)
        return;

    startProcessingCycle();
    for (int i = 0; i < total; i++) {
        // Validate operation against access mode
        QVariant category_variant = getObserverPropertyValue(observerData->subject_list.at(0),OBJECT_CATEGORY);
        QString category_string = category_variant.toString();
        if (!isConst(category_string)) {
            delete observerData->subject_list.at(0);
        }
    }
    endProcessingCycle();
}

QVariant Qtilities::Core::Observer::getObserverPropertyValue(const QObject* obj, const char* property_name) const {
    #ifndef QT_NO_DEBUG
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return QVariant();
    #endif

    QVariant prop;
    prop = obj->property(property_name);

    if (prop.isValid() && prop.canConvert<SharedObserverProperty>()) {
        // This is a shared property
        return (prop.value<SharedObserverProperty>()).value();
    } else if (prop.isValid() && prop.canConvert<ObserverProperty>()) {
        // This is a normal observer property (not shared)
        return (prop.value<ObserverProperty>()).value(observerData->observer_id);
    } else if (!prop.isValid()) {
        return QVariant();
    } else {
        LOG_TRACE(QString("Observer (%1): Getting of property (%2) failed, property not recognized as an observer property type.").arg(objectName()).arg(property_name));
        return QVariant();
    }
}

bool Qtilities::Core::Observer::setObserverPropertyValue(QObject* obj, const char* property_name, const QVariant& new_value) const {
    #ifndef QT_NO_DEBUG
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return false;
    #endif

    QVariant prop;
    prop = obj->property(property_name);

    // Important, we do not just use the setValue() functions on the ObserverProperties, we call
    // obj->setProperty to make sure the QDynamicPropertyChangeEvent event is triggered.
    if (prop.isValid() && prop.canConvert<SharedObserverProperty>()) {
        // This is a shared property
        SharedObserverProperty shared_property = prop.value<SharedObserverProperty>();
        shared_property.setValue(new_value);
        setSharedProperty(obj,shared_property);
        return true;
    } else if (prop.isValid() && prop.canConvert<ObserverProperty>()) {
        // This is a normal observer property (not shared)
        ObserverProperty observer_property = prop.value<ObserverProperty>();
        observer_property.setValue(new_value,observerData->observer_id);
        setObserverProperty(obj,observer_property);
        return true;
    } else {
        LOG_FATAL(QString(tr("Observer (%1): Setting the value of property (%2) failed. This property is not yet set as an ObserverProperty type class.")).arg(objectName()).arg(property_name));
        // Assert here, otherwise you will think that the property is being set and you won't understand why something else does not work.
        // If you get here, you need to create the property you need, and then set it using the setObserverProperty() or setSharedProperty() calls.
        // This function is not the correct one to use in your situation.
        Q_ASSERT(0);
    }

    return false;
}

void Qtilities::Core::Observer::removeObserverProperties(QObject* obj) const {
    #ifndef QT_NO_DEBUG
        Q_ASSERT(obj != 0);
    #endif
    #ifdef QT_NO_DEBUG
        if (!obj)
            return;
    #endif

    observerData->ignore_dynamic_property_changes = true;

    // This is usefull when you are adding properties and you encounter an error, in that case this function
    // can be used as sort of a rollback, removing the property changes that have been made up to that point.

    // Remove all the contexts first.
    foreach (QString property_name, monitoredProperties()) {
        ObserverProperty prop = getObserverProperty(obj, property_name.toStdString().data());
        if (prop.isValid()) {
            // If it exists, we remove this observer context
            prop.removeContext(observerData->observer_id);
            setObserverProperty(obj, prop);
        }
    }

    // If the count is zero after removing the contexts, remove all properties
    if (parentCount(obj) == 0) {
        foreach (QString property_name, monitoredProperties()) {
            if (property_name != QString(OBJECT_NAME))
                obj->setProperty(property_name.toStdString().data(),QVariant());
        }
    }

    observerData->ignore_dynamic_property_changes = false;
}

bool Qtilities::Core::Observer::isParentInHierarchy(const Observer* obj_to_check, const Observer* observer) const {
    // Get all the parents of observer
    ObserverProperty observer_map_prop = getObserverProperty(observer, OBSERVER_SUBJECT_IDS);
    int observer_count;
    if (observer_map_prop.isValid())
        observer_count = observer_map_prop.observerMap().count();
    else
        return false;

    if (observer_count == 0)
        return false;

    bool is_parent = false;
    for (int i = 0; i < observer_count; i++) {
        Observer* parent = QtilitiesCore::instance()->objectManager()->observerReference(observer_map_prop.observerMap().keys().at(i));

        if (parent != obj_to_check) {
            is_parent = isParentInHierarchy(obj_to_check,parent);
            if (is_parent)
                break;
        } else
            return true;
    }

    return is_parent;
}

bool Qtilities::Core::Observer::isConst(const QString& category) const {
    AccessMode mode;
    if (category.isEmpty() || (observerData->access_mode_scope == GlobalScope)) {
        mode = (AccessMode) observerData->access_mode;
    } else {
        if (observerData->category_access.keys().contains(category)) {
            mode = (AccessMode) observerData->category_access[category];
        } else {
            mode = (AccessMode) observerData->access_mode;
        }
    }

    if (mode == LockedAccess || mode == ReadOnlyAccess)
        return true;
    else
        return false;
}

bool Qtilities::Core::Observer::setSubjectLimit(int subject_limit) {
    // Check if this observer is read only
    if ((observerData->access_mode == ReadOnlyAccess || observerData->access_mode == LockedAccess) && observerData->access_mode_scope == GlobalScope) {
        LOG_ERROR(QString(tr("Setting the subject limit for observer \"%1\" failed. This observer is read only / locked.").arg(objectName())));
        return false;
    }

    if ((subject_limit < observerData->subject_list.count()) && (subject_limit != -1)) {
        LOG_ERROR(QString(tr("Setting the subject limit for observer \"%1\" failed, this observer is currently observing more subjects than the desired new limit.").arg(objectName())));
        return false;
    } else {
        observerData->subject_limit = subject_limit;
        setModificationState(true);
        return true;
    }
}

void Qtilities::Core::Observer::setAccessMode(AccessMode mode, const QString& category) {
    // Check if this observer is read only
    if (observerData->access_mode == ReadOnlyAccess || observerData->access_mode == LockedAccess) {
        LOG_ERROR(QString("Setting the access mode for observer \"%1\" failed. This observer is read only / locked.").arg(objectName()));
        return;
    }

    if (category.isEmpty())
        observerData->access_mode = (int) mode;
    else {
        observerData->category_access[category] = (int) mode;
    }
    setModificationState(true);
}

Qtilities::Core::Observer::AccessMode Qtilities::Core::Observer::accessMode(const QString& category) {
    if (category.isEmpty())
        return (AccessMode) observerData->access_mode;
    else {
        return (AccessMode) observerData->category_access[category];
    }
}

QString Qtilities::Core::Observer::observerName(int parent_id) const {
    if (parent_id == -1)
        return objectName();
    else {
        const Observer* obs = QtilitiesCore::instance()->objectManager()->observerReference(parent_id);
        if (obs) {
            if (obs->contains(obs))
                return obs->subjectNameInContext(this);
            else
                return obs->subjectNameInContext(parent());
        } else {
            return objectName();
        }
    }
}

QString Qtilities::Core::Observer::subjectNameInContext(const QObject* obj) const {
    if (!obj)
        return QString();

    // Check if the object is in this context
    if (contains(obj) || contains(obj->parent())) {
        // We need to check if a subject has a instance name in this context. If so, we use the instance name, not the objectName().
        QVariant instance_name = getObserverPropertyValue(obj,INSTANCE_NAMES);
        if (instance_name.isValid())
            return instance_name.toString();
        else
            return obj->objectName();
    }

    return QString();
}

int Qtilities::Core::Observer::childCount(const Observer* observer) const {
    static int child_count;

    // We need to iterate over all child observers recursively
    if (!observer) {
        child_count = 0;
        observer = this;
    }

    Observer* child_observer = 0;

    for (int i = 0; i < observer->subjectCount(); i++) {
        ++child_count;

        // Handle the case where the child is an observer.
        child_observer = qobject_cast<Observer*> (observer->subjectAt(i));
        if (child_observer)
            childCount(child_observer);
        else {
            // Handle the case where the child is the parent of an observer
            foreach (QObject* child, observer->subjectAt(i)->children()) {
                child_observer = qobject_cast<Observer*> (child);
                if (child_observer)
                    childCount(child_observer);
            }
        }
    }

    return child_count;
}

QStringList Qtilities::Core::Observer::subjectNames(const QString& iface) const {
    QStringList subject_names;

    for (int i = 0; i < observerData->subject_list.count(); i++) {
        if (observerData->subject_list.at(i)->inherits(iface.toAscii().data()) || iface.isEmpty()) {
            // We need to check if a subject has a instance name in this context. If so, we use the instance name, not the objectName().
            QVariant instance_name = getObserverPropertyValue(observerData->subject_list.at(i),INSTANCE_NAMES);
            if (instance_name.isValid())
                subject_names << instance_name.toString();
            else
                subject_names << observerData->subject_list.at(i)->objectName();
        }
    }
    return subject_names;
}

QStringList Qtilities::Core::Observer::subjectNamesByCategory(const QString& category) const {
    QStringList subject_names;

    for (int i = 0; i < observerData->subject_list.count(); i++) {
        ObserverProperty category_property = getObserverProperty(subjectAt(i),OBJECT_CATEGORY);
        if (category_property.isValid() && !category_property.value(observerID()).toString().isEmpty()) {
            if (category_property.value(observerID()).toString() == category) {
                // We need to check if a subject has a instance name in this context. If so, we use the instance name, not the objectName().
                QVariant instance_name = getObserverPropertyValue(observerData->subject_list.at(i),INSTANCE_NAMES);
                if (instance_name.isValid())
                    subject_names << instance_name.toString();
                else
                    subject_names << observerData->subject_list.at(i)->objectName();
            }
        } else {
            if (category == tr("Uncategorized")) {
                // We need to check if a subject has a instance name in this context. If so, we use the instance name, not the objectName().
                QVariant instance_name = getObserverPropertyValue(observerData->subject_list.at(i),INSTANCE_NAMES);
                if (instance_name.isValid())
                    subject_names << instance_name.toString();
                else
                    subject_names << observerData->subject_list.at(i)->objectName();
            }
        }
    }

    return subject_names;
}

QStringList Qtilities::Core::Observer::subjectCategories() const {
    QStringList subject_names;

    for (int i = 0; i < observerData->subject_list.count(); i++) {
        ObserverProperty category_property = getObserverProperty(subjectAt(i),OBJECT_CATEGORY);
        if (category_property.isValid() && !category_property.value(observerID()).toString().isEmpty()) {
            if (!subject_names.contains(category_property.value(observerID()).toString()))
                subject_names << category_property.value(observerID()).toString();
        } else {
            if (!subject_names.contains(tr("Uncategorized")))
                subject_names << tr("Uncategorized");
        }
    }

    return subject_names;
}

QObject* Qtilities::Core::Observer::subjectAt(int i) const {
    if ((i < 0) || i >= subjectCount())
        return 0;
    else
        return observerData->subject_list.at(i);
}

int Qtilities::Core::Observer::subjectID(int i) const {
    if (i < subjectCount()) {
        QVariant prop = getObserverPropertyValue(observerData->subject_list.at(i),OBSERVER_SUBJECT_IDS);
        return prop.toInt();
    } else
        return -1;
}

QList<QObject*> Qtilities::Core::Observer::subjectReferences(const QString& iface) const {
    QList<QObject*> subjects;
    for (int i = 0; i < observerData->subject_list.count(); i++) {
        if (iface.isEmpty())
            subjects << observerData->subject_list.at(i);
        else {
            if (observerData->subject_list.at(i)->inherits(iface.toAscii().data()))
                subjects << observerData->subject_list.at(i);
        }
    }
    return subjects;
}

QList<QObject*> Qtilities::Core::Observer::subjectReferencesByCategory(const QString& category) const {
    // Get all subjects which has the OBJECT_CATEGORY property set to category.
    QList<QObject*> list;
    for (int i = 0; i < observerData->subject_list.count(); i++) {
        ObserverProperty category_property = getObserverProperty(subjectAt(i),OBJECT_CATEGORY);
        if (category_property.isValid() && !category_property.value(observerID()).toString().isEmpty()) {
            if (category_property.value(observerID()).toString() == category)
                list << subjectAt(i);
        } else {
            if (category == tr("Uncategorized"))
                list << subjectAt(i);
        }
    }

    return list;
}


QMap<QPointer<QObject>, QString> Qtilities::Core::Observer::subjectMap() {
    QMap<QPointer<QObject>, QString> subject_map;
    for (int i = 0; i < observerData->subject_list.count(); i++) {
        QPointer<QObject> object_ptr = observerData->subject_list.at(i);
        subject_map[object_ptr] = subjectNameInContext(observerData->subject_list.at(i));
    }
    return subject_map;
}

QObject* Qtilities::Core::Observer::subjectReference(int ID) const {
    for (int i = 0; i < observerData->subject_list.count(); i++) {
        QObject* obj = observerData->subject_list.at(i);
        QVariant prop = getObserverPropertyValue(obj,OBSERVER_SUBJECT_IDS);
        if (!prop.isValid()) {
            LOG_TRACE(QString("Observer (%1): Looking for subject ID (%2) failed, property 'Subject ID' contains invalid variant for this context.").arg(objectName()).arg(ID));
            return 0;
        }
        if (prop.toInt() == ID)
            return obj;
    }
    return 0;
}

QObject* Qtilities::Core::Observer::subjectReference(const QString& subject_name) const {
    for (int i = 0; i < observerData->subject_list.count(); i++) {
        QObject* obj = observerData->subject_list.at(i);
        QVariant prop = getObserverPropertyValue(obj,OBJECT_NAME);
        if (!prop.isValid()) {
            LOG_TRACE(QString("Observer (%1): Looking for subject name (%2) failed, property 'Subject Name' contains invalid variant for this context.").arg(objectName()).arg(subject_name));
            return 0;
        }
        if (prop.toString() == subject_name)
            return obj;
    }
    return 0;
}

bool Qtilities::Core::Observer::contains(const QObject* object) const {
    // Problems with const, temp fix
    // return observerData->subject_list.contains(object);

    for (int i = 0; i < observerData->subject_list.count(); i++) {
        if (observerData->subject_list.at(i) == object)
            return true;
    }

    return false;
}

void Qtilities::Core::Observer::installSubjectFilter(AbstractSubjectFilter* subject_filter) {
    if (subjectCount() > 0) {
        LOG_ERROR(QString(tr("Observer (%1): Subject filter installation failed. Can't install subject filters if subjects is already attached to an observer.")).arg(objectName()));
        return;
    }

    observerData->subject_filters.append(subject_filter);

    // Set the observer context of the filter
    subject_filter->setObserverContext(this);
    subject_filter->setParent(this);

    // Connect signals
    Q_ASSERT(connect(subject_filter,SIGNAL(notifyDirtyProperty(const char*)),SIGNAL(propertyBecameDirty(const char*))) == true);

    setModificationState(true);
}

void Qtilities::Core::Observer::uninstallSubjectFilter(AbstractSubjectFilter* subject_filter) {
    if (subjectCount() > 0) {
        LOG_ERROR(QString(tr("Observer (%1): Subject filter uninstallation failed. Can't uninstall subject filters if subjects is already attached to an observer.")).arg(objectName()));
        return;
    }

    observerData->subject_filters.removeOne(subject_filter);

    // Set the observer context of the filter
    subject_filter->setObserverContext(0);
    subject_filter->setParent(0);
    subject_filter->disconnect(this);

    setModificationState(true);
}

QList<Qtilities::Core::AbstractSubjectFilter*> Qtilities::Core::Observer::subjectFilters() const {
    return observerData->subject_filters;
}

void Qtilities::Core::Observer::setNamingControlHint(Observer::NamingControl naming_control) {
    observerData->display_hints->naming_control = naming_control;
    setModificationState(true);
}

Qtilities::Core::Observer::NamingControl Qtilities::Core::Observer::namingControlHint() const {
    return observerData->display_hints->naming_control;
}

void Qtilities::Core::Observer::setActivityDisplayHint(Observer::ActivityDisplay activity_display) {
    observerData->display_hints->activity_display = activity_display;
    setModificationState(true);
}

Qtilities::Core::Observer::ActivityDisplay Qtilities::Core::Observer::activityDisplayHint() const {
    return observerData->display_hints->activity_display;
}

void Qtilities::Core::Observer::setActivityControlHint(Observer::ActivityControl activity_control) {
    observerData->display_hints->activity_control = activity_control;
    setModificationState(true);
}

Qtilities::Core::Observer::ActivityControl Qtilities::Core::Observer::activityControlHint() const {
    return observerData->display_hints->activity_control;
}

void Qtilities::Core::Observer::setItemSelectionControlHint(Observer::ItemSelectionControl item_selection_control) {
    observerData->display_hints->item_selection_control = item_selection_control;
    setModificationState(true);
}

Qtilities::Core::Observer::ItemSelectionControl Qtilities::Core::Observer::itemSelectionControlHint() const {
    return observerData->display_hints->item_selection_control;
}

void Qtilities::Core::Observer::setHierarchicalDisplayHint(Observer::HierarhicalDisplay hierarhical_display) {
    observerData->display_hints->hierarhical_display = hierarhical_display;
    setModificationState(true);
}

Qtilities::Core::Observer::HierarhicalDisplay Qtilities::Core::Observer::hierarchicalDisplayHint() const {
    return observerData->display_hints->hierarhical_display;
}

void Qtilities::Core::Observer::setDisplayFlagsHint(Observer::DisplayFlags display_flags) {
    observerData->display_hints->display_flags = display_flags;
    setModificationState(true);
}

Qtilities::Core::Observer::DisplayFlags Qtilities::Core::Observer::displayFlagsHint() const {
    return observerData->display_hints->display_flags;
}

void Qtilities::Core::Observer::setItemViewColumnFlags(Observer::ItemViewColumnFlags item_view_column_hint) {
    observerData->display_hints->item_view_column_hint = item_view_column_hint;
    setModificationState(true);
}

Qtilities::Core::Observer::ItemViewColumnFlags Qtilities::Core::Observer::itemViewColumnFlags() const {
    return observerData->display_hints->item_view_column_hint;
}

void Qtilities::Core::Observer::setActionHints(Observer::ActionHints action_hints) {
    observerData->display_hints->action_hints = action_hints;
    setModificationState(true);
}

Qtilities::Core::Observer::ActionHints Qtilities::Core::Observer::actionHints() const {
    return observerData->display_hints->action_hints;
}

void Qtilities::Core::Observer::setDisplayedCategories(const QStringList& category_list, bool inversed) {
    observerData->display_hints->category_list = category_list;
    observerData->display_hints->inverse_categories = inversed;

    // Will update views connected to this signal.
    if (!observerData->process_cycle_active) {
        setModificationState(true);
    }
}

QStringList Qtilities::Core::Observer::displayedCategories() {
    return observerData->display_hints->category_list;
}

bool Qtilities::Core::Observer::hasInversedCategoryDisplay() {
    return observerData->display_hints->inverse_categories;
}

bool Qtilities::Core::Observer::categoryFilterEnabled() {
    return observerData->display_hints->category_filter_enabled;
}

void Qtilities::Core::Observer::setCategoryFilterEnabled(bool enabled) {
    if (enabled != observerData->display_hints->category_filter_enabled) {
        observerData->display_hints->category_filter_enabled = enabled;
        setModificationState(true);
    }
}