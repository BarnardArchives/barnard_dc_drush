<?php

require_once 'includes/bc_islandora.drush_ingest_methods.inc';

/**
 * Implements hook_drush_command().
 */
function bc_islandora_ingest_drush_command() {
  return array(
    'ingest_objects' => array(
      'description' => 'Ingests objects.',
      'arguments' => array(
        'query_file' => 'A csv file containing information about the objects to be ingested.',
      ),
      'options' => array(
        'type' => 'The type of objects to ingest.',
        'year' => 'Specific year from which to ingest objects.',
        'month' => 'Month from which to ingest objects.',
        'generate-csv' => 'Whether to generate a csv file for use in newspaper ingest.',
        'identifier' => 'The identifier of the collection to ingest.',
      ),
      'examples' => array(
        'drush -u 1 --uri=http://localhost bdcio /path/to/pages.csv',
      ),
      'aliases' => array('bciio'),
      // We can pass in users id on the command line using drush -u.
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
    ),
  );
}

function drush_bc_islandora_ingest_objects($query_file) {
  if ($type = drush_get_option('type')) {
    $function = "_bc_islandora_drush_ingest_{$type}";
  }
  else {
    drush_log(dt("Please specify type of object to ingest."), 'error');
    return;
  }

  $options = array(
    'year' => drush_get_option('year'),
    'month' => drush_get_option('month'),
    'generate-csv' => drush_get_option('generate-csv'),
    'identifier' => drush_get_option('identifier'),
  );

  if (function_exists($function)) {
    $function($query_file, $options);
  }
  else {
    drush_log(dt("Didn't get ingest method at type = @type", array('@type' => $type)), 'error');
  }
}

