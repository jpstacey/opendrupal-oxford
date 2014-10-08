<?php
/**
 * @file
 * opendrupal_access.features.user_role.inc
 */

/**
 * Implements hook_user_default_roles().
 */
function opendrupal_access_user_default_roles() {
  $roles = array();

  // Exported role: administrator.
  $roles['administrator'] = array(
    'name' => 'administrator',
    'weight' => 2,
  );

  // Exported role: editor.
  $roles['editor'] = array(
    'name' => 'editor',
    'weight' => 4,
  );

  // Exported role: moderator.
  $roles['moderator'] = array(
    'name' => 'moderator',
    'weight' => 3,
  );

  // Exported role: persona.
  $roles['persona'] = array(
    'name' => 'persona',
    'weight' => 6,
  );

  // Exported role: subscriber.
  $roles['subscriber'] = array(
    'name' => 'subscriber',
    'weight' => 5,
  );

  return $roles;
}
