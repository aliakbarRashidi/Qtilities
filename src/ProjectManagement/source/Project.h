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

#ifndef PROJECT_H
#define PROJECT_H

#include "ProjectManagement_global.h"
#include "IProject.h"

#include <QObject>

namespace Qtilities {
    namespace ProjectManagement {
        using namespace Qtilities::ProjectManagement::Interfaces;
        /*!
          \struct ProjectData
          \brief The ProjectData class stores private data used by the Project class.
         */
        struct ProjectData;

        /*!
          \class Project
          \brief The Project class provides a ready to use implementation of the IProject plugin.

          When creating a new project using the ProjectManager class it will create an instance of this class
          and set it as the current open project.
         */
        class PROJECT_MANAGEMENT_SHARED_EXPORT Project : public QObject, public IProject
        {
            Q_OBJECT
            Q_INTERFACES(Qtilities::ProjectManagement::Interfaces::IProject)

        public:
            Project(QObject* parent = 0);
            ~Project();

            // --------------------------------------------
            // IProject Implementation
            // --------------------------------------------
            bool newProject();
            bool loadProject(const QString& file_name, bool close_current_first = true);
            bool saveProject(const QString& file_name);
            bool closeProject();
            QString projectFile() const;
            QString projectName() const;

            void setProjectItems(QList<IProjectItem*> project_items, bool inherit_modification_state = false);
            void addProjectItem(IProjectItem* project_item, bool inherit_modification_state = false);
            void removeProjectItem(IProjectItem* project_item);
            QStringList projectItemNames() const;
            int projectItemCount() const;
            IProjectItem* projectItem(int index);

            // --------------------------------
            // IModificationNotifier Implemenation
            // --------------------------------
            bool isModified() const;
            QObject* modifierBase() { return this; }
        public slots:
            void setModificationState(bool new_state, bool notify_listeners = true, bool notify_subjects = false);
        signals:
            void modificationStateChanged(bool is_modified) const;
            void partialStateChanged(const QString& part_name) const;

        public:
            // --------------------------------
            // IObjectBase Implemenation
            // --------------------------------
            QObject* objectBase() { return this; }

        private:
            ProjectData* d;
        };
    }
}

#endif // PROJECT_H