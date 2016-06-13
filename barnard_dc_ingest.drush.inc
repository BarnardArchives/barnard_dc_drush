<?php

require_once 'includes/barnard_dc.drush_ingest_helpers.inc';

/**
 * Implements hook_drush_command().
 */
function barnard_dc_ingest_drush_command() {
  return array(
    'ingest_newspapers' => array(
      'description' => "Creates the parent issue object and uploads the pages as child objects. Requires the Drupal7 version of Islandora with tuque",
      'arguments' => array(
        'query_file' => 'The path to a CSV file that contains the information about the newspaper pages.',
      ),
      'options' => array(
        'derivatives' => 'Whether to create derivatives.',
        'overwrite-existing' => 'Whether to overwrite existing objects.',
        'purge' => 'Whether to purge instead of ingesting.',
      ),
      'examples' => array(
        'drush -u 1 --uri=http://localhost ingest_newspapers /path/to/pages.csv',
      ),
      'aliases' => array('bciin', 'bdcin'),
      // We can pass in users id on the command line using drush -u.
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
    ),
    'ingest_books' => array(
      'description' => 'Ingests books.',
      'arguments' => array(
        'query_file' => 'A csv file containing information about the books to be ingested.',
      ),
      'options' => array(
        'derivatives' => 'Whether to create derivatives.',
        'overwrite-existing' => 'Whether to overwrite existing objects.',
        'purge' => 'Whether to purge instead of ingesting.',
        'year' => 'Whether to import a specific year\'s book.',
      ),
      'examples' => array(
        'drush -u 1 --uri=http://localhost bciib /path/to/pages.csv',
      ),
      'aliases' => array('bciib', 'bdcib'),
      // We can pass in users id on the command line using drush -u.
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
    ),
    'ingest_objects' => array(
      'description' => 'Ingests objects.',
      'arguments' => array(
        'query_file' => 'A csv file containing information about the objects to be ingested.',
      ),
      'options' => array(
        'type' => 'The type of objects to ingest.',
        'year' => 'Specific year from which to inget objects.',
      ),
      'examples' => array(
        'drush -u 1 --uri=http://localhost bdcio /path/to/pages.csv',
      ),
      'aliases' => array('bdcio'),
      // We can pass in users id on the command line using drush -u.
      'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_LOGIN,
    ),
    'generate_csv' => array(
      'description' => 'Generates csv files for ingesting objects.',
      'arguments' => array(
        'directory' => 'The directory containing image files.',
      ),
      'options' => array(
        'year' => 'The year for which to generate csv.',
        'month' => 'The month for which to generate csv.',
        'type' => 'The type of object for which to generate csv.',
      ),
      'examples' => array(
        'drush barnard_dc_generate_csv /home/vagrant/bulletin',
        'drush barnard_dc_generate_csv --year=2001 /home/vagrant/bulletin',
        'drush barnard_dc_generate_csv --year=2001 --month=08 /home/vagrant/bulletin',
      ),
      'aliases' => array('bcigc', 'bdcgc'),
    ),
  );
}

/**
 * Ingests book objects.
 *
 * @param string $query_file
 *   A csv file containing data about the newspaper(s) to be ingested.
 */
function drush_barnard_dc_ingest_books($query_file) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');

  ini_set("auto_detect_line_endings", "1");

  $fh = fopen($query_file, 'r');
  $year = drush_get_option('year');
  $books = array();

  while (($data = fgetcsv($fh)) !== FALSE) {
    $csv_year = $data[3];
    if ($year && $year != $csv_year) {
      continue;
    }
    // Some tricky stuff follows to handle books with multiple "volumes."
    $vol_parts = explode(' ', $data[1]);
    array_shift($vol_parts);
    $book_pid = 'bc:yearbook-' . implode('.', $vol_parts);
    $pages = $data[12];
    if (drush_get_option('purge')) {
      drush_barnard_dc_purge_object($book_pid);
    }
    elseif (!islandora_object_load($book_pid)) {
      try {
        print "Got book pid: $book_pid\n";
        $book_pid = create_book_object($book_pid, $data);
      }
      catch (Exception $e) {
        drush_log(dt('Caught exception: @e', array('@e' => $e->getMessage())), 'warning');
      }
    }
    elseif (drush_get_option('overwrite-existing')) {
      try {
        drush_barnard_dc_purge_object($book_pid);
        create_book_object($book_pid, $data);
      }
      catch (Exception $e) {
        drush_log(dt('Caught exception: @e', array('@e' => $e->getMessage())), 'warning');
      }
    }
    else {
      drush_log(dt('Issue exists at pid = @pid; skipping...', array('@pid' => $book_pid)), 'warning');
      continue;
    }
    $books[] = $book_pid;

    // Create each page.
    $file_path = dirname($query_file) . '/' . $csv_year;
    if (file_exists($file_path)) {
      $files = explode("\n", `ls $file_path/*.jp2`);
      foreach ($files as $fn) {
        if (!empty($fn)) {
          $page_number = preg_replace('/^0+/', '', preg_replace('/\.jp2$/', '', array_pop(explode('_', $fn))));
          $data[] = $page_number;
          $page_pid = $book_pid . '-' . $page_number;
          if (drush_get_option('overwrite-existing') || drush_get_option('purge')) {
            try {
              drush_barnard_dc_purge_object($page_pid);
            }
            catch (Exception $e) {
              drush_log(dt('Caught exception: @e', array('@e' => $e->getMessage())), 'warning');
            }
          }
          if (!drush_get_option('purge')) {
            create_page_object($fn, $data, $book_pid, $page_pid, 'book');
          }
        }
      }
    }

    // Create derivatives.
    if (drush_get_option('derivatives')) {
      _barnard_dc_create_derivatives($books);
    }
  }
}

/**
 * Ingests newspaper objects.
 *
 * @param string $query_file
 *   A csv file containing data about the newspaper(s) to be ingested.
 */
function drush_barnard_dc_ingest_newspapers($query_file) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');

  ini_set("auto_detect_line_endings", "1");

  $path_name = dirname($query_file);
  $fh = fopen($query_file, 'r');
  $line_number = 0;
  $issues = array();

  while (($data = fgetcsv($fh)) !== FALSE) {
    $line_number++;
    // Only parse the data if we aren't on the first line, which contains
    // column headings.
    if ($line_number > 1) {
      $type = $data[4];
      if ($type == 'issue') {
        $issue_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1]));

        // Either purge, create, or overwrite (purge then create) the object,
        // depending which options were specified on the command line.
        if (drush_get_option('purge')) {
          drush_barnard_dc_purge_object($issue_pid);
        }
        elseif (!islandora_object_load($issue_pid)) {
          try {
            $issue_pid = create_issue_object($issue_pid, $data);
          }
          catch (Exception $e) {
            drush_log(dt('Caught exception: @e', array('@e' => $e->getMessage())), 'warning');
          }
        }
        elseif (drush_get_option('overwrite-existing')) {
          try {
            drush_barnard_dc_purge_object($issue_pid);
            create_issue_object($issue_pid, $data);
          }
          catch (Exception $e) {
            drush_log(dt('Caught exception: @e', array('@e' => $e->getMessage())), 'warning');
          }
        }
        else {
          drush_log(dt('Issue exists at pid = @pid; skipping...', array('@pid' => $issue_pid)), 'warning');
        }
        $issues[] = $issue_pid;
      }
      elseif ($type == 'page') {
        $page_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1])) . '-' . $data[3];

        if (drush_get_option('purge')) {
          drush_barnard_dc_purge_object($page_pid);
        }
        elseif (!islandora_object_load($page_pid)) {
          try {
            create_page_object($path_name, $data, $issue_pid, $page_pid, 'newspaper');
          }
          catch (Exception $e) {
            drush_log(dt('Caught exception: @e', array('@e' => $e->getMessage())), 'warning');
          }
        }
        elseif (drush_get_option('overwrite-existing')) {
          try {
            drush_barnard_dc_purge_object($page_pid);
            create_page_object($path_name, $data, $issue_pid, $page_pid, 'newspaper');
          }
          catch (Exception $e) {
            drush_log(dt('Caught exception: @e', array('@e' => $e->getMessage())), 'warning');
          }
        }
        else {
          drush_log(dt('Page exists at pid = @pid; skipping...', array('@pid' => $page_pid)), 'warning');
        }
      }
    }
  }

  // Create derivatives.
  if (drush_get_option('derivatives')) {
    _barnard_dc_create_derivatives($issues);
  }
}

function drush_barnard_dc_ingest_objects($query_file) {
  if ($type = drush_get_option('type')) {
    $function = "_barnard_dc_drush_ingest_{$type}";
  }
  else {
    drush_log(dt("Didn't get type."), 'error');
    return;
  }

  $options = array(
    'year' => drush_get_option('year'),
  );

  if (function_exists($function)) {
    $function($query_file, $options);
  }
  else {
    drush_log(dt("Didn't get ingest method at type = @type", array('@type' => $type)), 'error');
  }
}

/**
 * Generates csv files for ingesting newspapers.
 */
function drush_barnard_dc_generate_csv($directory = '/home/BCIT/dsavage/bulletin') {
  $year = drush_get_option('year');
  $month = drush_get_option('month');
  $type = drush_get_option('type') ? drush_get_option('type') : 'newspaper';
  $function = "_barnard_dc_drush_generate_{$type}_csv";

  if (function_exists($function)) {
    $function($directory, array('year' => $year, 'month' => $month));
  }
}
