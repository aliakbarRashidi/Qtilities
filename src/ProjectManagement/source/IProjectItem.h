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

#ifndef IPROJECTITEM_H
#define IPROJECTITEM_H

#include "ProjectManagement_global.h"

#include <IModificationNotifier.h>

#include <QObject>
#include <QString>

using namespace Qtilities::Core::Interfaces;

namespace Qtilities {
    namespace ProjectManagement {
        namespace Interfaces {
            /*!
            \class IProjectItem
            \brief Interface through which objects can be exposed as project parts.
              */
            class PROJECT_MANAGEMENT_SHARED_EXPORT IProjectItem : virtual public IModificationNotifier {
            public:
                IProjectItem() {}
                virtual ~IProjectItem() {}

                //! The name of the project item.
                virtual QString projectItemName() const = 0;
                //! Called by the project manager when a new project needs to be created.
                virtual bool newProjectItem() = 0;
                //! Load the project from the specified file.
                virtual bool loadProjectItem(QDataStream& stream) = 0;
                //! Save the project to a specified file.
                virtual bool saveProjectItem(QDataStream& stream) = 0;
                //! Close the project.
                virtual bool closeProjectItem() = 0;                 
            };
        }
    }
}

Q_DECLARE_INTERFACE(Qtilities::ProjectManagement::Interfaces::IProjectItem,"com.ProjectManagement.IProjectItem/1.0");

#endif // IPROJECTITEM_H