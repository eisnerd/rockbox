/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2010 Robert Bieber
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#ifndef RBRENDERINFO_H
#define RBRENDERINFO_H

#include <QMap>

class RBScreen;
class ProjectModel;
class ParseTreeModel;

class RBRenderInfo
{
public:
    RBRenderInfo(ParseTreeModel* model,  ProjectModel* project,
                 QMap<QString, QString>* settings, RBScreen* screen);
    RBRenderInfo(const RBRenderInfo& other);
    virtual ~RBRenderInfo();

    const RBRenderInfo& operator=(const RBRenderInfo& other);

    ProjectModel* project() const{ return mProject; }
    QMap<QString, QString>* settings() const{ return mSettings; }
    RBScreen* screen() const{ return mScreen; }
    ParseTreeModel* model() const{ return mModel; }

private:
    ProjectModel* mProject;
    QMap<QString, QString>* mSettings;
    RBScreen* mScreen;
    ParseTreeModel* mModel;
};

#endif // RBRENDERINFO_H
