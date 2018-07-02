<?php

/**
 * @file
 * The main class of the Payer for working with transactions.
 */

class PayerTransaction {

  /*
   * Transaction details. Possible array keys:
   *   - ['tid']: transaction TID. Transmitted if you want to work with an
   *      existing transaction;
   *   - ['uid']: user UID. Example: $user->uid;
   *   - ['status']: transaction status. Can be "0" (Waiting) or
   *       "1" (Completed);
   *   - ['act']: transaction action. Can be "0" (Spending) or "1" (Coming);
   *   - ['amount']: the amount of replenishment or cancellation from the user's
   *       wallet. Should be a floating number with a decimal separator
   *       in the form of a dot;
   *   - ['method']: selected payment method. Plain text string.
   *       Example: manual, robokassa, paypal;
   *   - ['info']: array of values required for a specific payment method;
   *   - ['created']: date of transaction creation. Should be in Unixtime.
   */
  private $transaction = array();
  
  /*
   * Used when updating an existing transaction, for example: 0 (Pending)
   * changed to 1 (Completed). Will store: 0.
   */
  private $old_status;
  
  // User object Drupal:
  private $user;
  
  // The amount of money on the user's account. User field "payer_wallet":
  private $balance;
  
  /*
   * How much money will remain after the action with the wallet:
   *   a) when wasting money: ['act'] = 0 - difference $wallet and $amount;
   *   b) when replenishing money: ['act'] = 1 - sum $wallet and $amount.
   */
  private $sum_balance;
  
  /**
   * PayerTransaction constructor.
   *
   * @param array
   *   $data transaction
   *
   * @see load_transaction()
   */
  public function __construct($data) {
    if (isset($data['tid'])) {
      $this->transaction['tid'] = $data['tid'];
    }
    elseif (gettype($data) !== 'array' && $data > 0) {
      $this->transaction['tid'] = $data;
    }
    
    if (isset($this->transaction['tid'])) {
      $this->transaction = $this->load_transaction();
    }
    else {
      $this->transaction['uid'] = $data['uid'];
      $this->transaction['act'] = $data['act'];
      $this->transaction['status'] = $data['status'];
      $this->transaction['amount'] = $data['amount'];
      $this->transaction['method'] = $data['method'];
      $this->transaction['created'] = $data['created'];
      $this->transaction['info'] = $data['info'];
    }
  }
  
  /**
   * Save or update transaction.
   *
   * @see save_transaction()
   * @see update_transaction()
   * @see handler_user_balance()
   */
  public function save() {
    if (!isset($this->transaction['tid'])) {
      $this->save_transaction();
      if($this->transaction['status'] == 1) {
        $this->handler_user_balance();
      }
    }
    else {
      $this->update_transaction();
      if($this->old_status == 0 && $this->transaction['status'] == 1) {
        $this->handler_user_balance();
      }
    }
  }
  
  /**
   * Set transaction data.
   *
   * @param array
   *   $data - array transaction data.
   *   Only ['info'] and ['status'] keys are allowed.
   */
  public function set_transaction_data($data) {
    if (isset($data['status'])) {
      $this->old_status = $this->transaction['status'];
      $this->transaction['status'] = $data['status'];
    }
    
    if (isset($data['info'])) {
      $this->transaction['info'] = $data['info'];
    }
  }
  
  /**
   * Returns transaction properties from the current object as an array.
   *
   * @return array
   *   $this->transaction.
   */
  public function get_transaction() {
    return $this->transaction;
  }
  
  /**
   * Get a transaction from the database as an array.
   */
  private function load_transaction() {
    return db_select('payer_transactions', 't')
      ->fields('t', array(
        'tid',
        'uid',
        'status',
        'act',
        'amount',
        'created',
        'info',
        'method',
      ))
      ->condition('t.tid', $this->transaction['tid'])
      ->execute()
      ->fetchAssoc();
  }
  
  /**
   * Saving a new transaction to the database.
   */
  private function save_transaction() {
    $this->transaction['tid'] = db_insert('payer_transactions')
      ->fields($this->transaction)
      ->execute();
  }
  
  /**
   * Update transaction data. Only info and status are available for updating
   */
  private function update_transaction() {
    db_update('payer_transactions')
      ->fields(array(
        'status' => $this->transaction['status'],
        'info' => $this->transaction['info'],
      ))
      ->condition('tid', $this->transaction['tid'])
      ->execute();
  }
  
  /**
   * Get a user object by UID.
   **/
  private function get_user_object() {
    // If uid has not been transferred, load the current user:
    if (!isset($this->transaction['uid'])) {
      global $user;
      $this->transaction['uid'] = $user->uid;
    }
    
    $this->user = user_load($this->transaction['uid']);
  }
  
  /**
   * Record the calculated value of "$this->sum_balance" in the database.
   **/
  private function save_user_balance() {
    $this->user->payer_wallet[LANGUAGE_NONE][0]['value'] = $this->sum_balance;
    $field_info = field_info_field('payer_wallet');
    
    field_sql_storage_field_storage_write(
      'user',
      $this->user,
      'update',
      array($field_info['id'])
    );
    
    cache_clear_all("field:user:{$this->user->uid}", 'cache_field');
  }
  
  /**
   * Processes user balance. Performed if the status of
   * transaction 1 (Completed).
   *
   * @see get_user_object()
   * @see get_user_balance()
   * @see calc_sum_balance()
   * @see save_user_balance()
   *
   */
  private function handler_user_balance() {
    $this->get_user_object();
    $this->balance = self::get_user_balance($this->user);
    
    $this->sum_balance = self::calc_sum_balance(
      $this->transaction['act'],
      $this->balance,
      $this->transaction['amount']
    );
    
    $this->save_user_balance();
  }
  
  /**
   * Calculate account balance
   *
   * @param $act - transaction action. Can be "0" (Spending) or "1" (Coming);
   * @param $balance - current user balance;
   * @param $amount - amount of replenishment or write-offs.
   *
   * @return float
   *   $sum_balance
   */
  static public function calc_sum_balance($act, $balance, $amount) {
    if ($act == 0) {
      $sum_balance = $balance - $amount;
    }
    else {
      $sum_balance = $balance + $amount;
    }
    
    return $sum_balance;
  }
  
  /**
   * Getting the amount of money in the user's wallet.
   *
   * @param $user
   * @return $amount_wallet[0]['value']
   *
   */
  static public function get_user_balance($user) {
    $amount_wallet = field_get_items(
      'user',
      $user,
      'payer_wallet'
    );
    
    if (empty($amount_wallet[0]['value'])) {
      $amount_wallet[0]['value'] = 0;
    }
    
    return $amount_wallet[0]['value'];
  }

}
