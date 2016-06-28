<?php

/**
 * @file
 * Ingest methods for `drush bc_islandora_ingest_objects`.
 */

require_once 'bc_islandora.drush_ingest_helpers.inc';

/**
 * Ingests a newspaper object.
 *
 * Sample CSV data:
 * title,date,filename,"page number",type,extent
 * "Barnard Bulletin, October 1, 1990","October 1, 1990",,,issue,24
 */
function _bc_islandora_drush_ingest_newspaper($query_file, $options = array()) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');

  ini_set("auto_detect_line_endings", "1");

  // If the generate-csv option is specified or if the argument supplied is a
  // directory, generate a csv input file first.
  if ($options['generate-csv'] !== NULL || is_dir($query_file)) {
    $csv_file = _bc_islandora_drush_generate_newspaper_csv($query_file, $options);
    $fh = fopen($csv_file, 'r');
  }
  else {
    $fh = fopen($query_file, 'r');
  }

  $path_name = dirname($csv_file);
  $line_number = 0;
  $issues = array();

  if (!$fh) {
    drush_log("Couldn't open file.", 'error');
    return;
  }
  while (($data = fgetcsv($fh)) !== FALSE) {
    $line_number++;
    // Only parse the data if we aren't on the first line, which contains
    // column headings.
    if ($line_number > 1) {
      $type = $data[4];
      if ($type == 'issue') {
        // Sample issue pid: bc:bulletin-19901001.
        $issue_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1]));

        // If the issue doesn't exist, create it.
        if (!islandora_object_load($issue_pid)) {
          $issue_pid = create_issue_object($issue_pid, $data);
        }
        // If it exists, give the user the option to overwrite it.
        // (N.B. this process will also allow the user to remove all
        // associated issue pages.)
        else {
          $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $issue_pid)));
          if (preg_match('/^y/', strtolower($continue)) === 1) {
            drush_bc_islandora_purge_object($issue_pid);
            $issue_pid = create_issue_object($issue_pid, $data);
          }
        }
        $issues[] = $issue_pid;
      }
      elseif ($type == 'page') {
        // Sample page pid: bc:bulletin-19901001-1.
        $page_pid = 'bc:bulletin-' . date('Ymd', strtotime($data[1])) . '-' . $data[3];

        if (!islandora_object_load($page_pid)) {
          create_page_object($path_name, $data, $issue_pid, $page_pid, 'newspaper');
        }
        // If we got an existing page, just skip it without prompting.
        else {
          drush_log(dt('Page exists at pid = @pid; skipping...', array('@pid' => $page_pid)), 'warning');
        }
      }
    }
  }
  // TODO should we keep this?
  // _bc_islandora_create_derivatives($issues);
}

/**
 * Ingests a book.
 */
function _bc_islandora_drush_ingest_book($query_file, $options = array()) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  module_load_include('inc', 'islandora_paged_content', 'includes/batch');

  ini_set("auto_detect_line_endings", "1");

  $fh = fopen($query_file, 'r');
  $year = isset($options['year']) ? $options['year'] : NULL;
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

    if (!islandora_object_load($book_pid)) {
      drush_log("Creating book at pid = @pid", array('@pid' => $book_pid), 'ok');
      $book_pid = create_book_object($book_pid, $data);
    }
    else {
      $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $issue_pid)));
      if (preg_match('/^y/', strtolower($continue)) === 1) {
        drush_log(dt("Removing object: @pid", array('@pid' => $issue_pid)), 'ok');
        drush_bc_islandora_purge_object($issue_pid);
        $book_pid = create_book_object($book_pid, $data);
      }
      else {
        continue;
      }
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

          if (!islandora_object_load($page_pid)) {
            create_page_object($fn, $data, $book_pid, $page_pid, 'book');
          }
          else {
            drush_log(dt("Page exists at pid = @pid. Skipping...", array('@pid' => $page_pid)), 'warning');
          }
        }
      }
    }
    else {
      drush_log(dt("File not found: @file_path", array('@file_path' => $file_path)), 'error');
    }
  }
  // TODO should we keep this?
  _bc_islandora_create_derivatives($books);
}

/**
 * Ingests student publications.
 *
 * @param string $directory
 *   The directory containing publications to ingest.
 */
function _bc_islandora_drush_ingest_student_publication($directory, $options = array()) {
  $pub_id = $options['identifier'];

  if (is_dir($directory)) {
    // Look at each file in the supplied (argument) directory.
    foreach (scandir($directory) as $file) {
      // The full path to this file.
      $path = $directory . '/' . $file;
      // The XML data file (should follow the format: [directory name].xml).
      $xml_file = $path . '/' . $file . '.xml';
      // If we didn't get a good directory or XML file, move on.
      if (!is_dir($path) || preg_match('/^\./', $file) !== 0 || !file_exists($xml_file)) {
        continue;
      }
      $pub_data = _bc_islandora_pub_data($xml_file);
      $pid_title = str_replace(' ', '-', preg_replace('/[^a-z\d\s+]/', '', strtolower($pub_data['title'])));
      $pub_pid = 'bc:' . $pid_title;
      // Publication ids that constitute "exceptions" (different behavior
      // required).
      $exceptions = array('BC12-9_1959', 'BC12-26_1970', 'BC12-30_C19711972');
      $exception = in_array(array_pop(explode('/', $path)), $exceptions);
      // If pub object doesn't exist, create it.
      if (!islandora_object_load($pub_pid) && !$exception) {
        create_pub_object($pub_pid, $pub_data);
      }
      elseif ($exception) {
        $collection_pid = variable_get('bc_islandora_pubs_pid', 'islandora:1022');
        $book_pid = create_book_object($pub_pid, _bc_islandora_pub_data($xml_file, $collection_pid));
      }
      else {
        $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $pub_pid)));
        if (preg_match('/^y/', strtolower($continue)) === 1) {
          drush_log(dt("Removing object: @pub_pid", array('@pub_pid' => $pub_pid)), 'ok');
          drush_bc_islandora_purge_object($pub_pid);
          create_pub_object($pub_pid, $pub_data);
        }
      }
      $pub_obj = islandora_object_load($pub_pid);
      // If we got an id, only look at the corresponding dir.
      if (($pub_id && $file == $pub_id) || !$pub_id) {
        if ($exception) {
          // Yuk!
          if (!isset($book_pid)) {
            $book_pid = $pub_pid;
          }
          _bc_islandora_process_pub_pages($book_pid, $xml_file, $path, $purge);
        }
        // If non-exceptional, do the normal thing (TODO reverse this order?)
        else {
          // Create a book object corresponding to each pub.
          foreach (scandir($path) as $subfile) {
            $subpath = implode('/', array($path, $subfile));
            $xml_file = implode('/', array($subpath, $subfile)) . '.xml';
            if ((!is_dir($subpath) || preg_match('/^\./', $subdir) !== 0 || !file_exists($xml_file)) && !$exception) {
              continue;
            }
            $book_data = _bc_islandora_pub_data($xml_file, $pub_pid, 'book');
            $issue_pid = implode('-', array($pub_pid, $book_data['date']));
            if (!islandora_object_load($issue_pid)) {
              create_book_object($issue_pid, $book_data);
            }
            else {
              $continue = drush_prompt(dt("Object exists at pid = @pid. Overwrite? (y/n)", array('@pid' => $issue_pid)));
              if (preg_match('/^y/', strtolower($continue)) === 1) {
                drush_log(dt("Removing object: @pub_pid", array('@pub_pid' => $issue_pid)), 'ok');
                drush_bc_islandora_purge_object($issue_pid);
                create_book_object($issue_pid, $book_data);
              }
            }
            $issue_obj = islandora_object_load($issue_pid);
            // Create a page object for each page.
            _bc_islandora_process_pub_pages($issue_pid, $xml_file, $subpath);
          }
        }
      }
    }
  }
}

/**
 * Ingests manuscripts.
 */
function _bc_islandora_drush_ingest_manuscript($md_file, $options) {
  $id_option = $options['identifier'];

  if (file_exists($md_file)) {
    $md_fh = fopen($md_file, 'r');
    while (($data = fgetcsv($md_fh)) !== FALSE) {
      $ms_data = array();
      $ms_data['id'] = $data[0];
      // Skip this row if we got an id option and this id doesn't match.
      if ($id_option && $id_option != $ms_data['id']) {
        continue;
      }
      $ms_data_dir = dirname($md_file) . '/' . $ms_data['id'];
      if (is_dir($ms_data_dir)) {
        $ms_data['title'] = $data[1];
        $ms_mods_file = $ms_data_dir . '/' . $ms_data['id'] . '.xml';
        // If we found an XML file that matches the naming convention, assume
        // it contains the MODS data we want.
        if (file_exists($ms_mods_file)) {
          $ms_data['xml'] = file_get_contents($ms_mods_file);
        }
        // Otherwise, generate MODS using the other csv fields.
        else {
          $ms_data['creator'] = $data[2];
          $ms_data['creator_uri'] = $data[3];
          $ms_data['resource_type'] = $data[4];
          $ms_data['genre'] = $data[5];
          $ms_data['genre_uri'] = $data[6];
          $ms_data['date'] = $data[7];
          // $ms_data['date_qualifier'] = $data[8];
          // NB start and end date only present if date is approx range...
          $ms_data['date_start'] = $data[8];
          $ms_data['date_end'] = $data[9];
          $ms_data['extent'] = $data[10];
          $ms_data['description'] = $data[11];
          $ms_data['location'] = $data[12];
          $ms_data['topic'] = $data[13];
          $ms_data['topic_uri'] = $data[14];
        }

        $ms_pid = 'bcrw';

        if (!islandora_object_load($ms_pid)) {
          $ms_data['collection_pid'] = variable_get('bc_islandora_manuscripts_pid', 'islandora:manuscriptCollection');
          $ms_data['cm_pid'] = 'islandora:manuscriptCModel';
          $ms_data['xml'] = _bc_islandora_ms_mods($ms_data);
          // $ms_parent_pid = create_pub_object($ms_pid, $ms_data);
          $ms_parent_pid = create_pub_object('bcrw', $ms_data);
        }
        else {
          drush_log(dt('Object exists: @ms_pid', array('@ms_pid' => $ms_pid)), 'ok');
        }

        foreach (scandir($ms_data_dir) as $ms_file) {
          // Create a page object for each tif file we find.
          if (preg_match('/\.tif$/', $ms_file)) {
            $page_padded = str_replace('.tif', '', array_pop(explode('_', $ms_file)));
            $page_number = preg_replace('/^0+/', '', $page_padded);
            $page_pid = $ms_pid . '-' . $page_number;
            $filepath = $ms_data_dir . '/' . $ms_file;
            $ms_data['id'] = $data[0] . '_' . $page_padded;
            $ms_data['page_number'] = $page_number;
            $ms_data['title'] = $data[1] . ', page ' . $page_number;
            $ms_data['cm_pid'] = $ms_pid;
            $ms_data['type'] = 'page';
            $ms_data['xml'] = _bc_islandora_ms_mods($ms_data);

            // TODO this is redundant now. Find another way to check if page
            // exists?
            if (!islandora_object_load($page_pid)) {
              create_page_object($ms_data_dir . '/' . $ms_file, $ms_data, $ms_parent_pid, 'bcrw', 'ms');
            }
            else {
              drush_log(dt('Object exists: @page_pid', array('@page_pid' => $page_pid)), 'ok');
            }
          }
        }
      }
    }
  }
  else {
    drush_log(dt("File not found: @md_file", array('@md_file' => $md_file)), 'error');
  }
}
