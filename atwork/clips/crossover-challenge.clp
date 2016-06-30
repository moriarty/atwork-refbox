(defclass RCLLOrder (is-a USER) (role concrete)
  (slot next-id (type INTEGER) (storage shared) (default 0))
  
  (slot id (type INTEGER))
  (slot cap-color (type SYMBOL) (allowed-values CAP_BLACK CAP_GREY) (default CAP_BLACK))
  (slot quantity-requested (type INTEGER) (default 1))
  (slot quantity-delivered (type INTEGER) (default 0))
)

(defmessage-handler RCLLOrder init ()
  (call-next-handler)
  (modify-instance ?self (id ?self:next-id) (next-id (+ ?self:next-id 1)))
  (printout warn "Crossover order: " ?self:id ", " ?self:cap-color ", " ?self:quantity-requested crlf)
)

(defmessage-handler RCLLOrder create-msg ()
  "Create a ProtoBut message of an RCLLOrder"
  (bind ?o (pb-create "rci_pb_msgs.Order"))

  (pb-set-field ?o "id" ?self:id)
  (pb-set-field ?o "cap_color" ?self:cap-color)
  (pb-set-field ?o "quantity_requested" ?self:quantity-requested)
  (pb-set-field ?o "quantity_delivered" ?self:quantity-delivered)

  (return ?o)
)

(defrule net-recv-SetRCLLOrder
  ?mf <- (protobuf-msg (type "rci_pb_msgs.SetRCLLOrder") (ptr ?p) (rcvd-via STREAM))
  =>
  (retract ?mf) ; message will be destroyed after rule completes

  (bind ?pb-cap-color (pb-field-value ?p "cap_color"))
  (bind ?pb-quantity-requested (pb-field-value ?p "quantity_requested"))

  (printout warn "Received set crossover order" crlf)
  ; TODO protect this?
  (bind ?order (make-instance of RCLLOrder (cap-color ?pb-cap-color) (quantity-requested ?pb-quantity-requested)))

  (bind ?pb-order (send ?order create-msg))

  (do-for-all-facts ((?client network-client)) TRUE
    (pb-send ?client:id ?pb-order)
  )

  (slot-insert$ [task-info] tasks 1
     (make-instance of Task (status OFFERED) (task-type TRANSPORTATION)
       (transportation-task (make-instance of TransportationTask
         (object-id [M30])
         (quantity-requested ?pb-quantity-requested)
         (destination-id [workstation-06])
         (source-id [workstation-05])))
      )
   )
  (pb-destroy ?pb-order)
)

(defrule net-recv-RCLLOrder
  ?mf <- (protobuf-msg (type "rci_pb_msgs.Order") (ptr ?p) (rcvd-via STREAM))
  =>
  (retract ?mf) ; message will be destroyed after rule completes

  (bind ?pb-cap-color (pb-field-value ?p "cap_color"))
  (bind ?pb-quantity-delivered (pb-field-value ?p "quantity_delivered"))

  (printout warn "Received crossover delivery" crlf)
  ; TODO protect this?
  (slot-insert$ [inventory] items 1
    (make-instance of Item (object-id [M30]) (location-id [workstation-05]))
  )
)

; TODO - is this needed?
(defrule make-dummy-RCLLOrder
  (init)
  =>
  (make-instance [dummy-rcll-order] of RCLLOrder)
)
