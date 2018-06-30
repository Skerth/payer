<?php

/**
 * @file
 * Provides the Payer administrative interface.
 */

/**
 * PAGE SETTINGS
 */

/**
 * Form of settings where you can change the URL of the purse replenishment page.
 *
 * @see payer_settings_submit()
 */
function payer_settings_form($form) {
  $form['payer_payment_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Payment url'),
    '#description' => t("Use a relative path and don't add a trailing slash or the URL won't work."),
    '#default_value' => variable_get('payer_payment_url'),
    '#required' => TRUE,
    '#maxlength' => 255,
  ); /**
   * Wrapper for URLs
   */
  $form['main_settings']['urls'] = array(
    '#type' => 'fieldset',
    '#title' => t("URLs"),
    '#description' => t("Use a relative path and don't add a trailing slash or the URL won't work."),
    '#weight' => 1,
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#tree' => FALSE,
  );

  $form['main_settings']['urls']['payer_payment_success_url'] = array(
    '#type' => 'textfield',
    '#title' => t('URL success'),
    '#field_prefix' => $_SERVER['SERVER_NAME'] . '/',
    '#default_value' => variable_get('payer_payment_success_url', ''),
    '#maxlength' => 255,
  );

  $form['main_settings']['urls']['payer_payment_fail_url'] = array(
    '#type' => 'textfield',
    '#title' => t('URL fail'),
    '#field_prefix' => $_SERVER['SERVER_NAME'] . '/',
    '#default_value' => variable_get('payer_payment_fail_url', ''),
    '#maxlength' => 255,
  );

  $form = system_settings_form($form);
  array_unshift($form['#submit'], 'payer_settings_form_submit');

  return $form;
}

/**
 * Module configuration form handler.
 *
 * @see payer_settings()
 */
function payer_settings_form_submit($form, &$form_state) {
  $urls = array(
    'payment' => variable_get('payer_payment_url'),
    'success' => variable_get('payer_payment_success_url'),
    'fail' => variable_get('payer_payment_fail_url'),
  );

  // If any of the url is different, rebuild the menu:
  $is_url_payment_different = $form_state['values']['payer_payment_url'] != $urls['payment'];
  $is_url_success_different = $form_state['values']['payer_payment_success_url'] != $urls['success'];
  $is_url_fail_different = $form_state['values']['payer_payment_fail_url'] != $urls['fail'];

  if ($is_url_payment_different || $is_url_success_different || $is_url_fail_different) {
    menu_rebuild();
    drupal_set_message(t('The menu has been rebuilt.'));
  }
}


/**
 * PAGE TRANSACTIONS LIST
 */

/**
 * Contains a list of possible transaction filters.
 * @return array $filters
 */
function payer_transaction_filters() {
  $keys = array(
    'tid',
    'username',
    'act',
    'status',
    'items_per_page',
  );

  $filters = array_fill_keys($keys, NULL);

  return $filters;
};

/**
 * Applies filters for transactions administration filters based on session.
 *
 * @param $query
 *   A SelectQuery to which the filters should be applied.
 */
function payer_build_filter_transactions_query(SelectQueryInterface $query, $session) {
  // Build conditions query
  foreach ($session as $filter => $options) {
    if ($filter == 'username') {
      $user = user_load_by_name($session['username']);
      if ($user != FALSE) {
        $query->condition('t.uid', $user->uid);
      }
    }
    else {
      $query->condition('t.' . $filter, $session[$filter]);
    }
  }
}

/**
 * Output of all available transactions with sorting capability.
 *
 * @see payer_build_filter_transactions_query()
 * @see payer_transactions_list_filter_form()
 * @see payer_transactions_list_filter_form_submit()
 * @see payer_theme()
 */
function payer_transactions_list() {
  $session = isset($_SESSION['payer_transaction_overview_filter']) ? $_SESSION['payer_transaction_overview_filter'] : array();

  $header = array(
    array('data' => 'ID', 'field' => 'tid'),
    array('data' => 'User', 'field' => 'uid'),
    array('data' => 'Amount', 'field' => 'amount'),
    array('data' => 'Act', 'field' => 'act'),
    array('data' => 'Status', 'field' => 'status'),
    array('data' => 'Method', 'field' => 'method'),
    array('data' => 'Created', 'field' => 'created'),
    array('data' => 'Info', 'field' => 'info'),
  );

  // Generate query to database
  $query = db_select('payer_transactions', 't');
  $query->fields('t',
      array(
        'tid',
        'uid',
        'created',
        'amount',
        'act',
        'status',
        'method',
        'info',
      )
    )
    ->extend('PagerDefault')
    ->limit(isset($session['items_per_page']) ? $session['items_per_page'] : 25)
    ->extend('TableSort')
    ->orderByHeader($header)
    ->execute();

  unset($session['items_per_page']);
  payer_build_filter_transactions_query($query, $session);
  $transactions = $query->execute();

  $rows = array();
  foreach ($transactions as $transaction) {
    $user = user_load($transaction->uid);
    $user_link = l($user->name, url('user/' . $user->uid));

    $rows[] = array(
      l($transaction->tid, url('admin/config/payer/transactions/' . $transaction->tid)),
      $user_link,
      array(
        'data'  => $transaction->amount,
        'class' => array('transaction-amount', 'transaction-amount-' . $transaction->act)
      ),
      $transaction->act ? 'Coming' : 'Spending',
      $transaction->status ? 'Completed' : 'Waiting',
      $transaction->method,
      format_date($transaction->created, 'short'),
      $transaction->info,
    );
  }
  $filter_form = drupal_get_form('payer_transactions_list_filter_form');

  $page_form_filter = drupal_render($filter_form);
  $page_data = theme('table', array('header' => $header, 'rows' => $rows, 'empty' => t('No transaction available.')));
  $page_data .= theme('pager');


  return theme(
    'payer_template_transactions_list',
    array(
      'page_form_filter' => $page_form_filter,
      'page_data' => $page_data,
    )
  );
}

/**
 * Description of the form of the filtering of transactions.
 *
 * @see payer_transactions_list_filter_form_submit()
 *
 * @param $form
 * @return mixed $form
 */
function payer_transactions_list_filter_form($form) {
  $session = isset($_SESSION['payer_transaction_overview_filter']) ? $_SESSION['payer_transaction_overview_filter'] : array();

  $form['filters'] = array(
    '#type' => 'fieldset',
    '#title' => t('Show only items where'),
    '#weight' => 0,
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
    '#tree' => FALSE,
    '#attributes' => array(
      'class' => array('clearfix')
    ),
  );

  $form['filters']['items'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('clearfix')),
  );

  $form['filters']['items']['filters'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('exposed-filters')),
  );

  $form['filters']['items']['filters']['tid'] = array(
    '#type' => 'textfield',
    '#title' => t('Transaction ID'),
    '#size' => '23',
    '#default_value' => isset($session['tid']) ? $session['tid'] : '',
    '#maxlength' => '60',
  );

  $form['filters']['items']['filters']['username'] = array(
    '#type' => 'textfield',
    '#title' => t('Username'),
    '#autocomplete_path' => 'user/autocomplete',
    '#size' => '23',
    '#default_value' => isset($session['username']) ? $session['username'] : '',
    '#maxlength' => '60',
  );

  $form['filters']['items']['filters']['act'] = array(
    '#type' => 'select',
    '#title' => t('Act'),
    '#default_value' => isset($session['act']) ? $session['act'] : '',
    '#options' => array(
      '[any]' => t('- Any -'),
      0 => t('Spending'),
      1 => t('Coming'),
    ),
  );

  $form['filters']['items']['filters']['status'] = array(
    '#type' => 'select',
    '#title' => t('Status'),
    '#default_value' => isset($session['status'])  ? $session['status'] : '',
    '#options' => array(
      '[any]' => t('- Any -'),
      0 => t('Waiting'),
      1 => t('Completed'),
    ),
  );

  $form['filters']['items']['filters']['items_per_page'] = array(
    '#type' => 'select',
    '#title' => t('Items per page'),
    '#default_value' => isset($session['items_per_page'])  ? $session['items_per_page'] : 25,
    '#options' => array(
      10 => '10',
      25 => '25',
      50 => '50',
      100 => '100',
      250 => '250',
      500 => '500',
    ),
  );

  $form['filters']['items']['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Filter'),
  );

  if (count($session)) {
    $form['filters']['items']['actions']['reset'] = array(
      '#type' => 'submit',
      '#value' => t('Reset'),
    );
  }
  $form['#submit'][] = 'payer_transactions_list_filter_form_submit';

  return $form;
}

/**
 * Submit handler of the transactions filter form.
 *
 * @see payer_transactions_list_filter_form()
 * @see payer_transaction_filters()
 */
function payer_transactions_list_filter_form_submit($form, &$form_state) {
  if ($form_state['values']['op'] == t("Reset")) {
    $_SESSION['payer_transaction_overview_filter'] = array();
  }
  else {
    $filters = payer_transaction_filters();
    foreach ($filters as $filter => $options) {
      if ($form_state['values'][$filter] != '' && $form_state['values'][$filter] != '[any]') {
        $_SESSION['payer_transaction_overview_filter'][$filter] = $form_state['values'][$filter];
      }
      else {
        unset($_SESSION['payer_transaction_overview_filter'][$filter]);
      }
    }
  }
}


/**
 * PAGE VIEW TRANSACTION
 */

/**
 * Output all information about a transaction.
 *
 * @see PayerTransaction
 * @see payer_theme()
 */
function payer_transaction_view($tid) {
  $transaction = new PayerTransaction($tid);
  $transaction = $transaction->get_transaction();


  if ($transaction) {
    $page_data = array();

    foreach ($transaction as $key => $data) {
      switch ($key) {
        case 'uid':
          $user = user_load($data);
          $page_data['user']['label'] = t('User');
          $page_data['user']['value'] = l($user->name, 'user/' . $data);
          break;

        case 'created':
          $page_data['created']['label'] = t('Created');
          $page_data['created']['value'] = format_date($data, 'short');
          break;

        case 'act':
          $page_data['act']['label'] = t('Act');
          $page_data['act']['value'] = $data?'Coming':'Spending';
          break;

        case 'status':
          $page_data['status']['label'] = t('Status');
          $page_data['status']['value'] = $data?'Completed':'Waiting';
          break;

        case 'amount':
          $page_data['amount']['label'] = t('Amount');
          $page_data['amount']['value'] = $data;
          $page_data['amount']['class'] = ' transaction-amount transaction-amount-' . $transaction['act'];
          break;

        case 'info':
          $info = '';
          /*foreach ($data as $key_item => $item) {
            $info .= ucfirst($key_item) . ': ';
            $info .= "$item; ";
          }*/
          $page_data['info']['label'] = 'Info';
          $page_data['info']['value'] = $data;
          break;

        default:
          $page_data[$key]['label'] = ucfirst($key);
          $page_data[$key]['value'] = $data;
          break;
      }
    }

    if ($page_data['status']['value'] == 'Waiting') {
      $get_payment_link = 'payer_' . $transaction['method'] . '_get_payment_link';
      if (function_exists($get_payment_link)) {
        $page_data['payment_link']['label'] = 'Payment link';
        $page_data['payment_link']['value'] = $get_payment_link($transaction);
      }
    }

    return theme(
      'payer_template_transaction_view',
      array('page_data' => $page_data)
    );
  }

  return drupal_not_found();
}


/**
 * PAGE ADD TRANSACTION
 */

/**
 * Description of the form of adding a transaction
 *
 * @see payer_add_transaction_validate()
 * @see payer_add_transaction_submit()
 */
function payer_add_transaction_form($form) {
  $form['payer_transaction_amount'] = array(
    '#type' => 'textfield',
    '#title' => t('Amount'),
    '#element_validate' => array('element_validate_number'),
    '#required' => TRUE,
    '#maxlength' => 20,
    '#size' => 20,
  );

  $form['payer_transaction_act'] = array(
    '#type' => 'radios',
    '#title' => t('Act'),
    '#options' => array(
      0 => t('Spending'),
      1 => t('Coming'),
    ),
    '#required' => TRUE,
  );

  $form['payer_transaction_status'] = array(
    '#type' => 'radios',
    '#title' => t('Status'),
    '#options' => array(
      0 => t('Waiting'),
      1 => t('Completed'),
    ),
    '#required' => TRUE,
  );

  $form['payer_transaction_method'] = array(
    '#type' => 'textfield',
    '#title' => t('Method'),
    '#maxlength' => 255,
    '#default_value' => t('Manually'),
  );

  $form['payer_transaction_info'] = array(
    '#type' => 'textfield',
    '#title' => t('Info'),
    '#maxlength' => 255,
  );

  $form['payer_transaction_username'] = array(
    '#type' => 'textfield',
    '#title' => t('Username'),
    '#description' => t('Leave blank for the current user'),
    '#default_value' => '',
    '#autocomplete_path' => 'user/autocomplete',
    '#size' => '20',
    '#maxlength' => '60',
  );

  $form['actions']['custom_submit'] = array(
    '#type' => 'submit',
    '#value' => t('Create transaction'),
  );

  $form['#validate'] = array('payer_add_transaction_form_validate');
  $form['#submit'] = array('payer_add_transaction_form_submit');

  return $form;
}

/**
 * Validation of the form of adding a transaction
 *
 * @see payer_add_transaction_form()
 * @see PayerTransaction
 */
function payer_add_transaction_form_validate($form, &$form_state) {
  $act = $form_state['values']['payer_transaction_act'];
  $amount = $form_state['values']['payer_transaction_amount'];

  if ($form_state['values']['payer_transaction_username'] == '') {
    form_set_error('payer_transaction_username', t('User not found.'));
  }
  else {
    $user = user_load_by_name($form_state['values']['payer_transaction_username']);

    if (!empty($amount)) {
      // Class PayerTransaction is in the file "includes/payer.transaction.inc".
      $balance = PayerTransaction::get_user_balance($user);
      $sum_balance = PayerTransaction::calc_sum_balance($act, $balance, $amount);

      if (isset($amount) && $act == 0 && $sum_balance < 0) {
        form_set_error('payer_transaction_amount', t('Insufficient funds!'));
      }
    }
  }
}

/**
 * Submit handler of the form of adding a transaction
 *
 * @see payer_add_transaction_form()
 * @see PayerTransaction
 */
function payer_add_transaction_form_submit($form, &$form_state) {
  $username = $form_state['values']['payer_transaction_username'];

  if (empty($username)) {
    global $user;
  }
  else {
    $user = user_load_by_name($form_state['values']['payer_transaction_username']);
  }

  $vars = array();
  $vars['uid'] = $user->uid;
  $vars['status'] = $form_state['values']['payer_transaction_status'];
  $vars['act'] = $form_state['values']['payer_transaction_act'];

  $vars['amount'] = $form_state['values']['payer_transaction_amount'];
  $vars['amount'] = number_format($vars['amount'], 2, '.', '');

  $vars['method'] = $form_state['values']['payer_transaction_method'];
  $vars['info'] = $form_state['values']['payer_transaction_info'];
  $vars['created'] = REQUEST_TIME;

  $transaction = new PayerTransaction($vars);
  $transaction->save();
}


/**
 * PAGE METHODS PAYMENT LIST
 */

/**
 * Handler of the pages of the list of methods of the module Payer
 *
 * @see payer_get_methods()
 * @see payer_theme()
 **/
function payer_methods_list() {
  $existing_methods = payer_get_methods();
  // Table header with content
  $headers = array(
    array('class' => array('pay-method_status')),
    array('data' => 'Method'),
    array('data' => 'Operations'),
  );

  // Table rows with content
  $rows = array();

  /**
   * Preparation of information on cash payment methods
   * in the folder "payer/methods"
   */
  foreach ($existing_methods as $pay_method) {
    $options = array(
      'config_link' => NULL, // ссылка на конфиг метода оплаты.
      'method_name' => NULL, // системное название метода.
      'description' => NULL, // описание метода.
      'image' => NULL, // изображение.
      'info' => NULL, // информация, содержащаяся в файле .info
      'status' => FALSE, // Флаг активности модуля
    );

    $options['info'] = drupal_parse_info_file($pay_method->uri);
    $options['method_name'] = $pay_method->name;
    $options['status'] = module_exists($options['method_name']) ? TRUE : FALSE;

    if (!$options['status']) {
      $options['description'] .= '<div class="messages warning">';
      $options['description'] .= t(
        'The payment method &quot;@pay_method&quot; is installed, but it is off. To use the module functionality, enable it on the !page_modules under &quot;Payment&quot;',
        array(
          '@pay_method' => $options['info']['name'],
          '!page_modules' => l(t('modules page'), 'admin/modules'),
        )
      );
      $options['description'] .= '</div>';
      $options['status'] = '🚫';
    }
    else {
      $options['config_link'] = l(t('Config'), $options['info']['configure']);
      $options['status'] = '✅';
    }

    $options['image'] = drupal_get_path('module', $options['method_name']) . '/preview.png';

    if (file_exists($options['image'])) {
      $options['image'] = theme('image', array('path' => $options['image']));
    }
    else {
      $options['image'] = NULL;
    }

    $options['description'] .= '<h3>' . $options['info']['name'] . '</h3>';
    $options['description'] .= theme('default', array());;
    $options['description'] .= $options['image'];
    $options['description'] .= '<p>' . $options['info']['description'] . '</p>';

    // Добавление вычесленных данных в массив $rows
    $rows[$options['method_name']] = array(
      'data' => array(
        array(
          'data'  => $options['status'],
          'class' => array('pay-method_status')
        ),
        $options['description'],
        $options['config_link'],
      ),
    );
  }

  usort($rows, function($a, $b) {
    if ($a['data'][0]['data'] === $b['data'][0]['data']) {
      return 0;
    }

    return $a['data'][0]['data'] > $b['data'][0]['data'] ? 1 : -1;
  });

  // Assembling data for a page
  $page_data = array(
    'header' => $headers,
    'rows' => $rows,
    'sticky' => TRUE,
    'empty' => t('No results found'),
    'attributes' => array(),
    'caption' => FALSE,
    'colgroups' => array(),
  );

  return theme(
    'payer_template_methods_list',
    array('page_data' => $page_data)
  );
}
