/**
 * Support post-install task forms.
 */
(function($) {

var username = "site.admin";

/**
 * Register custom state change: devmode.
 */
$(document).bind('state:devmode', function(e) {
  if (e.trigger) {
    // If we're in dev mode, populate fields.
    if (e.value) {
      $('#edit-username').val(username);
      $('#edit-email').val(username + "@example.com");
      $('#edit-password-pass1').val(username);
      $('#edit-password-pass2').val(username);
    }
    else {
      $('#edit-username').val("");
      $('#edit-email').val("");
      $('#edit-password-pass1').val("");
      $('#edit-password-pass2').val("");
    }
  }
});

})(jQuery);
