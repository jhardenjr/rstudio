/*
 * SessionUserPrefs.cpp
 *
 * Copyright (C) 2009-19 by RStudio, Inc.
 *
 * Unless you have received this program directly from RStudio pursuant
 * to the terms of a commercial license agreement with RStudio, then
 * this program is licensed to you under the terms of version 3 of the
 * GNU Affero General Public License. This program is distributed WITHOUT
 * ANY EXPRESS OR IMPLIED WARRANTY, INCLUDING THOSE OF NON-INFRINGEMENT,
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE. Please refer to the
 * AGPL (http://www.gnu.org/licenses/agpl-3.0.txt) for more details.
 *
 */

#include <core/system/Xdg.hpp>
#include <core/FileSerializer.hpp>
#include <core/json/JsonRpc.hpp>
#include <core/json/rapidjson/schema.h>

#include <session/SessionOptions.hpp>

#include "SessionUserPrefs.hpp"

using namespace rstudio::core;

namespace rstudio {
namespace session {
namespace modules {
namespace prefs {
namespace {

static boost::shared_ptr<json::Object> s_pUserPrefs;

}

json::Object userPrefs()
{
   if (s_pUserPrefs)
      return *s_pUserPrefs;
   return json::Object();
}

Error initialize()
{
   // Load schema for validation
   FilePath schemaFile = 
      options().rResourcesPath().complete("prefs").complete("user-prefs-schema.json");
   std::string schemaContents;
   Error error = readStringFromFile(schemaFile, &schemaContents);
   if (error)
      return error;

   // Extract default values from schema
   json::Object defaults;
   error = json::getSchemaDefaults(schemaContents, &defaults);
   if (error)
      return error;

   // If there's a system-wide configuration file, load that first.
   FilePath globalPrefsFile = core::system::xdg::systemConfigDir().complete(kUserPrefsFile);
   if (globalPrefsFile.exists())
   {
      std::string globalPrefsContents;
      error = readStringFromFile(globalPrefsFile, &globalPrefsContents);
      if (error)
      {
         // Non-fatal; we will proceed with defaults and/or user prefs.
         LOG_ERROR(error);
      }
      else
      {
         // We have a global preferences file; ensure it's valid.
         json::Value globalPrefs;
         error = json::parseAndValidate(globalPrefsContents, schemaContents, ERROR_LOCATION, 
               &globalPrefs);
         if (error)
         {
            LOG_ERROR(error);
         }
         else if (globalPrefs.type() == json::ObjectType)
         {
            // Overlay the globally specified values over the defaults.
            defaults = json::merge(defaults.get_obj(), globalPrefs.get_obj());
         }
      }
   }

   // Use the defaults unless there's a valid user prefs file.
   s_pUserPrefs = boost::make_shared<json::Object>(defaults);

   FilePath prefsFile = core::system::xdg::userConfigDir().complete(kUserPrefsFile);
   if (!prefsFile.exists())
   {
      // No user prefs file, just use whatever defaults we've accumulated.
      return Success();
   }

   std::string prefsContents;
   error = readStringFromFile(prefsFile, &prefsContents);
   if (error)
   {
      // Don't fail here since it will cause startup to fail, we'll just live with no prefs.
      LOG_ERROR(error);
      return Success();
   }

   json::Value prefs;
   error = json::parseAndValidate(prefsContents, schemaContents, ERROR_LOCATION, &prefs);
   if (error)
   {
      // Invalid or non-conforming user prefs; use defaults.
      LOG_ERROR(error);
      return Success();
   }

   // Overlay user prefs over global prefs and defaults
   *s_pUserPrefs = json::merge(defaults, prefs.get_obj());

   return Success();
}

} // namespace prefs
} // namespace modules
} // namespace session
} // namespace rstudio
