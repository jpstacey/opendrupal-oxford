<?php

/**
 * @file
 * Page callbacks for post-install tasks.
 */

/**
 * Form API callback: post-install tasks as UID=1.
 */
function odinstall_pages_post_install_uid1($form, &$form_state) {
  // Site reset properties.
  $form['site_reset'] = array(
    '#type' => 'fieldset',
    '#title' => t("Initial site reset"),
    '#description' => t("Ensure the site is in a consistent initial state, following install."),
  );
  $form['site_reset']['rebuild_permissions'] = array(
    '#title' => t("Rebuild content permissions"),
    '#description' => t("Usually a formality, especially immediately after site build, this just fixes any inconsistent permissions on any content."),
    '#type' => 'checkbox',
    '#default_value' => node_access_needs_rebuild(),
  );
  $form['site_reset']['revert_features'] = array(
    '#title' => t("Revert features"),
    '#description' => t("Check any installed features are in the right initial state i.e. nothing has accidentally been left overridden during install."),
    '#type' => 'checkbox',
    '#default_value' => TRUE,
  );
  $form['site_reset']['production_mode'] = array(
    '#title' => t("Enable production mode"),
    '#description' => t("Sites are installed in development mode, with unblocked personas etc. Tick this box to disable all the development functionality and make the site 'production-ready'."),
    '#type' => 'checkbox',
  );

  // Put all user-related content in the same fieldset.
  $form['users'] = array(
    '#type' => 'fieldset',
    '#title' => t("User accounts"),
  );

  // New superuser details.
  $form['users']['new_user'] = array(
    '#type' => 'fieldset',
    '#title' => t("New user"),
    '#description' => t("You should never access the website as user UID=1: it can be dangerous and leaves very little 'paper trail'. Create a new, personal account to use instead."),
  );

  $form['users']['new_user']['username'] = array(
    '#type' => 'textfield',
    '#title' => t("Username"),
    '#description' => t("Pick a suitable username for your first user. It should be unique to a person, so we suggest <em>firstname.lastname</em>."),
  );
  $form['users']['new_user']['email'] = array(
    '#type' => 'textfield',
    '#title' => t("E-mail address"),
    '#description' => t("The email address for your user. Type this carefully, as this will be how you'll get a password reset if you need one."),
  );
  $form['users']['new_user']['password'] = array(
    '#type' => 'password_confirm',
    '#title' => t("Define a secure password for your user. They'll be an administrator, so try to make it strong."),
  );

  // Add states to the user fields, to auto-fill in dev mode. We only add the
  // state change to one field, and the JS references the others.
  foreach (array('username') as $user_info) {
    $form['users']['new_user'][$user_info]['#states'] = array(
      'devmode' => array('#edit-production-mode' => array('unchecked' => TRUE)),
    );
  }
  // And attach Javascript to support this auto-filling.
  $form['#attached']['js'][] = drupal_get_path('module', 'odinstall') . '/js/odinstall.js';

  // If a superuser account looks like it's already been created, collapse.
  $number_two = user_load(2);
  if ($number_two && $number_two->status) {
    $form['users']['new_user']['#collapsible'] = TRUE;
    $form['users']['new_user']['#collapsed'] = TRUE;
    $form['users']['new_user']['#description'] = t("Looks like you've already created at least one new user. If you want to create another, do so below.");
  }
  // Otherwise, make a non UID=1 user mandatory.
  else {
    foreach (element_children($form['users']['new_user']) as $user_info) {
      $form['users']['new_user'][$user_info]['#required'] = TRUE;
    }
  }

  // Persona users.
  $form['users']['persona_accounts'] = array(
    '#type' => 'checkbox',
    '#title' => t("Create persona users"),
    '#description' => t("Create users for each user role, with the persona role attached. These will be used for testing the site works as expected, but they'll be blocked on production sites."),
    '#default_value' => TRUE,
  );

  $form['go'] = array(
    '#value' => 'Run post-install superuser tasks',
    '#type' => 'submit',
  );

  return $form;
}

/**
 * Form API validate callback: check any post-install tasks for UID=1.
 */
function odinstall_pages_post_install_uid1_validate($form, &$form_state) {
  // Check that, if we have one bit of data for the UID=2, we have all.
  $has_data = array();
  foreach (array("username", "email", "password") as $user_data_key) {
    $has_data[$user_data_key] = strlen($form_state['values'][$user_data_key]);
  }
  if (!array_sum($has_data)) {
    return;
  }

  // We know we have some data, so flag errors on any empty ones.
  foreach ($has_data as $user_data_key => $length) {
    ($length == 0) && form_set_error(
      $user_data_key,
      t(
        "If any one superuser field is filled out, all the rest should be. The field %field is empty.",
        array("%field" => $user_data_key)
      )
    );
  }
}

/**
 * Form API submit callback: run post-install tasks as UID=1.
 */
function odinstall_pages_post_install_uid1_submit($form, &$form_state) {
  // We will probably use our API functions, so include them.
  module_load_include("api.inc", "odinstall");
  $v = $form_state['values'];

  // Site reset actions.
  if ($v['rebuild_permissions']) {
    node_access_rebuild(FALSE);
  }

  if ($v['revert_features']) {
    _odinstall_pages_revert_features();
  }

  if ($v['production_mode']) {
    odinstall_api_devmode_unset();
  }

  // Create new accounts.
  $new_accounts = array();

  // Create a new account to replace the UID=1 superuser.
  if ($v['username']) {
    odinstall_api_append_account_from_form_values($new_accounts, $v, array(
      variable_get('user_admin_role', 3) => 'administrator',
    ));
  }

  // Create accounts for persona users (if they don't exist).
  if ($v['persona_accounts']) {
    odinstall_api_add_persona_accounts($new_accounts, $v['production_mode']);
  }

  // Bulk-create all accounts, regardless of where they've come from.
  foreach ($new_accounts as $account_edit) {
    user_save(NULL, $account_edit);
  }

  drupal_set_message(t(
    'All post-install tasks done. Now <a href="@l">log out</a> and continue as your new admin user.',
    array('@l' => url('user/logout'))
  ));
}

/**
 * Form API callback: post-install tasks for an administrator.
 */
function odinstall_pages_post_install($form, &$form_state) {
  // We need the API for ships.
  module_load_include("api.inc", "odinstall");

  // Configuration of site variables, ship import etc.
  $form['config'] = array(
    '#type' => 'fieldset',
    '#title' => t("Configure installation"),
  );

  $form['config']['site_name'] = array(
    '#type' => 'textfield',
    '#title' => t("Site name"),
    '#description' => t("Core Drupal site name, as configured during installation."),
    '#default_value' => variable_get('site_name', 'My Site'),
    '#required' => TRUE,
  );

  $form['config']['site_mail'] = array(
    '#type' => 'textfield',
    '#title' => t("Site email"),
    '#description' => t("Core Drupal site email, as configured during installation."),
    '#default_value' => variable_get('site_mail', 'My Site'),
    '#required' => TRUE,
  );

  $form['config']['ship'] = array(
    '#type' => 'select',
    '#title' => t("Ship"),
    '#description' => t("Ship information to import for the initial build of the site. For simplicity, your site should ideally begin with only one ship, although you can import more ships later."),
    '#options' => odinstall_api_get_ships(),
    '#required' => TRUE,
  );

  $form['block_uid1'] = array(
    '#title' => t("Block superuser account"),
    '#description' => t("Block the superuser UID=1 account. Be sure the password for your non-superuser account is known."),
    '#type' => 'checkbox',
    '#default_value' => TRUE,
  );
  $form['disable_module'] = array(
    '#title' => t("Disable post-install module"),
    '#description' => t("Disable the module that provides this functionality (and the nagging messages)."),
    '#type' => 'checkbox',
    '#default_value' => TRUE,
  );

  $form['go'] = array(
    '#value' => 'Run post-install tasks',
    '#type' => 'submit',
  );

  return $form;
}

/**
 * Form API submit callback: post-install tasks for an administrator.
 */
function odinstall_pages_post_install_submit($form, &$form_state) {
  // Include API for functionality, and set form values to shorthand.
  module_load_include("api.inc", "odinstall");
  $v = $form_state['values'];

  // Configuration.
  variable_set("site_name", $v['site_name']);
  variable_set("site_mail", $v['site_mail']);

  // Disable user with UID=1.
  $account = user_load(1);
  if ($v['block_uid1']) {
    user_save($account, array("status" => 0));
    drupal_set_message("Superuser account UID=1 has been blocked.", "ok");
  }

  // Regardless of options, always warn the site admin to import ports.
  drupal_set_message(t(
    "Please remember to run your <a href='@url'>ports import</a> to make your site ready for use.",
    array("@url" => "admin/structure/taxonomy/ports/import")
  ), "warning");

  // Disable this module and any dependents.
  if ($v['disable_module']) {
    $modules = array("odinstall", "taxonomy_xml");
    module_disable($modules);
    drupal_uninstall_modules($modules);
    // Also clear caches for features etc: not implicitly done above.
    drupal_flush_all_caches();

    drupal_set_message("Module for post-install tasks has been disabled.", "ok");
    drupal_goto("");
  }
}

/**
 * Helper function to revert features and parse results.
 */
function _odinstall_pages_revert_features() {
  // Revert features and get results back.
  $results = odinstall_api_revert_features();
  // Single result - something went badly wrong.
  if (isset($results[0])) {
    drupal_set_message($results[0]["error"], "error");
  }
  // Multiple results: report to the user.
  else {
    $features_touched = 0;
    foreach ($results as $feature => $subresults) {
      foreach ($subresults as $status => $messages) {
        foreach ($messages as $component => $message) {
          if ($message == t("Current state already matches defaults, aborting.")) {
            continue;
          }
          $features_touched++;
          drupal_set_message(t(
            "Attempted revert of %feature: status %status; message '%message'",
            array(
              "%feature" => $feature,
              "%component" => $component,
              "%status" => $status,
              "%message" => $message,
            )
          ), $status);
        }
      }
    }
  }
  if (!$features_touched) {
    drupal_set_message(t("No features needed to be reverted."), "ok");
  }
}
