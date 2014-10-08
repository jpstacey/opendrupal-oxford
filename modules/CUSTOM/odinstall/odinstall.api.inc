<?php

/**
 * @file
 * API functions for use in install profile and afterwards.
 */

/**
 * Create a single menu item.
 */
function odinstall_api_create_menu_item($item) {
  $exists = db_query(
    "SELECT mlid from {menu_links} WHERE link_title=:link_title AND link_path=:link_path",
    array(':link_title' => $link['link_title'], ':link_path' => $link['link_path'])
  )->fetchField();

  // Save the record if the data does not exist.
  if (!$exists) {
    menu_link_save($item);
  }
}

/**
 * Turn a CSV file of menu items into actual items.
 */
function odinstall_api_create_menu_items_from_csv($filename) {
  // Attempt to parse CSV file; otherwise return NULL.
  $items = odinstall_api_parse_csv($filename);
  if ($items === NULL) {
    return;
  }

  // Loop over CSV items, turn numeric indexes into textual keys, and create.
  foreach ($items as $item_indexed) {
    $item = array(
      'link_path' => $item_indexed[0],
      'link_title' => $item_indexed[1],
      'menu_name' => $item_indexed[2],
      'weight' => $item_indexed[3],
      'expanded' => $item_indexed[4],
    );
    odinstall_api_create_menu_item($item);
  }
}

/**
 * Create all menu items expected by installation.
 */
function odinstall_api_create_menu_items() {
  odinstall_api_create_menu_items_from_csv(
    drupal_get_path("module", "odinstall") . "/data/menu_items.csv"
  );
}

/**
 * Parse a CSV file into a structured array.
 */
function odinstall_api_parse_csv($filename) {
  // Attempt to open the CSV file; otherwise return NULL.
  $file = @fopen($filename, "r");
  if (!$file) {
    return;
  }

  $rows = array();
  while (!feof($file)) {
    $row = fgetcsv($file);
    if ($row) {
      $rows[] = $row;
    }
  }

  return $rows;
}

/**
 * Revert all features on a build.
 */
function odinstall_api_revert_features($force = FALSE) {
  $features = odinstall_api_parse_csv(
    drupal_get_path("module", "odinstall") . "/data/features.csv"
  );
  if (!$features) {
    return array(array("error" => t('Could not parse CSV configuration.')));
  }

  // Turn into an array suitable for drush_features_revert().
  $modules = array();
  foreach ($features as $feature) {
    $modules[$feature[0]] = TRUE;
  }
  // Assemble results here.
  $results = array();

  // Cut from drush_features_revert(), almost verbatim.
  module_load_include('inc', 'features', 'features.export');
  features_include();

  // Snip...
  foreach ($modules as $module => $components_needed) {
    $dt_args['@module'] = $module;
    if (($feature = features_load_feature($module, TRUE)) && module_exists($module)) {

      $components = array();
      // Forcefully revert all components of a feature.
      if ($force) {
        foreach (array_keys($feature->info['features']) as $component) {
          if (features_hook($component, 'features_revert')) {
            $components[] = $component;
          }
        }
      }
      // Only revert components that are detected to be Overridden/Needs
      // review/rebuildable.
      else {
        $states = features_get_component_states(array($feature->name), FALSE);
        foreach ($states[$feature->name] as $component => $state) {
          $revertable_states = array(
            FEATURES_OVERRIDDEN,
            FEATURES_NEEDS_REVIEW,
            FEATURES_REBUILDABLE,
          );
          if (in_array($state, $revertable_states) && features_hook($component, 'features_revert')) {
            $components[] = $component;
          }
        }
      }

      if (!empty($components_needed) && is_array($components_needed)) {
        $components = array_intersect($components, $components_needed);
      }
      if (empty($components)) {
        $results[$module]["ok"]["none"] = t('Current state already matches defaults, aborting.');
      }
      else {
        foreach ($components as $component) {
          $dt_args['@component'] = $component;
          features_revert(array($module => array($component)));
          $results[$module]["ok"][$component] = t('Reverted @module.@component.', $dt_args);
        }
      }
    }
    elseif ($feature) {
      $results[$module]["warning"]["none"] = t('The feature @feature" is not enabled.', array("@feature" => $feature));
      // _features_drush_set_error($module, 'FEATURES_FEATURE_NOT_ENABLED');
    }
    else {
      return array(array("error" => t('Feature could not be found.')));
    }
  }
  return $results;
}

/**
 * Create all taxonomy terms during install phase.
 */
function odinstall_api_create_taxonomy_terms() {
  odinstall_api_create_taxonomy_terms_from_csv();
}

/**
 * Create many taxonomy terms using taxonomy_xml.
 */
function odinstall_api_create_taxonomy_terms_from_csv() {
  module_load_include('inc', 'taxonomy_xml', 'formats/csvancestry_format');
  module_load_include('inc', 'taxonomy_xml', 'formats/csv_format');

  $csv_file_prefix = drupal_get_path("module", "odinstall") . "/data/taxonomy_";

  $context = array();
  foreach (array(
    // "example_taxonomy",
  ) as $machine_name) {
    // Get text and vocabulary object.
    $text = file_get_contents("$csv_file_prefix$machine_name.csv");
    $vocabulary = taxonomy_vocabulary_machine_name_load($machine_name);

    // Function taxonomy_xml_csvancestry_parse() only works in batch mode, so we
    // reproduce its control flow here.
    $rows = explode("\n", $text);
    // Fake batch context.
    $context[$vocabulary->vid] = array();

    // Each row becomes a taxonomy term.
    foreach ($rows as $weight => $row) {
      // Try to get term name; quit if it's blank.
      $row_data = csv_string_to_array($row);
      $term_name = trim($row_data[0]);
      if (!$term_name) {
        continue;
      }

      // Even cvsancestry_import_row is rubbish: doesn't preserve weight.
      // (Yes, that's CVSancestry_import_row.)
      $term = (object) array(
        'vid' => $vocabulary->vid,
        'name' => $row_data[0],
        'weight' => $weight,
      );

      // Save term and add it to the context to return.
      taxonomy_term_save($term);
      $context[$vocabulary->vid]['new_terms'][] = $term;
    }
  }
  return $context;
}

/**
 * Append an account to an existing array of new accounts to add.
 */
function odinstall_api_append_account_from_form_values(&$new_accounts, $v, $roles) {
  $new_accounts[] = array(
    'name' => $v['username'],
    'mail' => $v['email'],
    'pass' => $v['password'],
    'status' => 1,
    'init' => $v['email'],
    'roles' => $roles,
  );
}

/**
 * Helper function for clarity: merge persona accounts into array.
 */
function odinstall_api_add_persona_accounts(&$new_accounts, $is_blocked = TRUE) {
  $roles = array();
  $persona_rid = NULL;
  $sample_usernames = array(
    "anna", "bob", "carole", "daniel",
    "eve", "frank", "georgia", "harry",
    "ida", "joseph", "kate", "len",
  );

  // Depending on role, assign to create-user array, or persona role, or drop.
  foreach (user_roles(TRUE) as $rid => $role_name) {
    switch ($role_name) {
      case "persona":
        $persona_rid = $rid;
        break;

      case "authenticated user":
        break;

      default:
        $roles[$rid] = $role_name;
    }
  }

  // For each array-to-create-user role, create users with persona role too.
  foreach ($roles as $rid => $role_name) {
    // How many users to create?
    $number_of_users = isset($users_per_role[$role_name]) ? $users_per_role[$role_name] : 1;
    for ($i = 0; $i < $number_of_users; $i++) {
      // Assemble the username and check the user doesn't already exist.
      $username = array_shift($sample_usernames) . ".persona";
      if (user_load_by_name($username)) {
        continue;
      }

      $new_accounts[] = array(
        'name' => $username,
        'mail' => "$username@example.com",
        'pass' => $username,
        'status' => !$is_blocked,
        'init' => "$username@example.com",
        'roles' => array(
          $rid => $role_name,
          $persona_rid => "persona",
        ),
      );
    }
  }
}

/**
 * Run tasks to turn site into "dev mode".
 */
function odinstall_api_devmode_set() {
  drupal_set_message("Putting site into development mode");

  // Enable some developer modules and configure them.
  module_enable(array("devel", "coder", "maillog"));
  variable_set("maillog_send", "0");
  variable_set("maillog_log", "1");
  variable_set("maillog_devel", "1");
}

/**
 * Run tasks to turn site into "production mode".
 */
function odinstall_api_devmode_unset() {
  $modules = array("devel", "coder", "maillog");
  module_disable($modules);
  drupal_uninstall_modules($modules);
}
