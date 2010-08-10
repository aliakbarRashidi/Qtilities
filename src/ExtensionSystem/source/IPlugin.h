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

#ifndef IPLUGIN_H
#define IPLUGIN_H

#include "ExtensionSystem_global.h"

#include <QObject>
#include <QList>

namespace Qtilities {
    namespace ExtensionSystem {
        //! Namespace containing available interfaces which forms part of the ExtensionSystem Module.
        namespace Interfaces {
            /*!
            \class IPlugin
            \brief Interface used to communicate with plugins.
              */
            class EXTENSION_SYSTEM_SHARED_EXPORT IPlugin : public QObject
            {
                Q_OBJECT
                Q_ENUMS(PluginState)
                Q_PROPERTY(PluginState State READ pluginState)
                Q_PROPERTY(QString ErrorString READ errorString)
                Q_PROPERTY(QString Description READ pluginDescription)
                Q_PROPERTY(double Version READ pluginVersion)
                Q_PROPERTY(QString Publisher READ pluginPublisher)
                Q_PROPERTY(QString PublisherWebsite READ pluginPublisherWebsite)
                Q_PROPERTY(QString PublisherContact READ pluginPublisherContact)
                Q_PROPERTY(QString Copyright READ pluginCopyright)
                Q_PROPERTY(QString License READ pluginLicense)

            public:
                IPlugin(QObject* parent = 0) : QObject(parent) {}
                virtual ~IPlugin() {}

                //! The possible states in which a plugin can be.
                enum PluginState {
                    Functional,             /*!< The plugin is fully functional. */
                    InitializationError,    /*!< The plugin detected an error during initialization. \sa initialize(). */
                    DependancyError         /*!< The plugin detected an error during depedancy initialization. \sa initializeDependancies(). */
                };
                //! Sets the plugin state.
                void setPluginState(PluginState state) { d_state = state; }
                //! Gets the plugin state.
                PluginState pluginState() { return d_state; }
                //! Sets the error string.
                void setErrorString(const QString& error_string) { d_error_string = error_string; }
                //! Gets the error state.
                QString errorString() { return d_error_string; }

                //! This function is called when the plugin is loaded.
                /*!
                    Use this implementation to register objects in the global object pool. Typically one plugin
                    will look for objects in other plugins which implements a specific interface. These objects,
                    the ones implementing the interfaces, must be added to the global object pool in this function.
                    */
                virtual bool initialize(const QStringList &arguments, QString *errorString) = 0;
                //! This function is called when all the plugins in the system were loaded and initialized.
                /*!
                    If you have an object which looks for specific interfaces in the global object pool, do the search
                    here.
                    */
                virtual bool initializeDependancies(QString *errorString) = 0;
                //! This function is called before a plugin is unloaded in the system.
                /*!
                    If your plugin needs to do something before it is unloaded (save settings for example), do it in here.
                    */
                virtual void finalize() { }

                //! The version of the plugin.
                virtual double pluginVersion() = 0;
                //! The version of the application for which the plugin is developed which the plugin was designed for.
                virtual QStringList pluginCompatibilityVersions() = 0;
                //! The name of the plugin's publisher.
                virtual QString pluginPublisher() = 0;
                //! The website of the plugin's publisher.
                virtual QString pluginPublisherWebsite() = 0;
                //! The contact details (in the form of an email address) of the plugin's publisher.
                virtual QString pluginPublisherContact() = 0;
                //! A description for the plugin.
                virtual QString pluginDescription() = 0;
                //! The copyright information of the plugin.
                virtual QString pluginCopyright() = 0;
                //! The licensing details of the plugin.
                virtual QString pluginLicense() = 0;

            private:
                PluginState d_state;
                QString d_error_string;
            };
        }
    }
}

Q_DECLARE_INTERFACE(Qtilities::ExtensionSystem::Interfaces::IPlugin,"com.qtilities.ExtensionSystem.IPlugin/1.0");

#endif // IPLUGIN_H