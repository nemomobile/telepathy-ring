;
; telepathy style for Emacs
;
(defconst my-telepathy-c-style
  '("gnu"
    (indent-tabs-mode . nil)
    (c-offsets-alist
     (brace-list-open . tp-brace-list-open)
     (arglist-intro . 4)
     (arglist-cont-nonempty . tp-lineup-arglist-cont)))
  "C Style for telepathy")

(defun tp-brace-list-open (langelem)
  (save-excursion
    (goto-char (cdr langelem))
    (if (looking-at "\\(\\btypedef\\b\\s-+\\)?\\benum\\b")
	0
      '+)))

(defun tp-lineup-arglist-cont (langelem)
  (let (syntax)
    (save-excursion
      (goto-char (cdr langelem))
      (setq syntax (c-guess-basic-syntax)))
    (if (assq 'topmost-intro-cont syntax)
	;; Lineup arglist in function definitions
	(c-lineup-arglist-intro-after-paren langelem)
      ;; 'topmost-intro is used in declarations
      4)))

(c-add-style "telepathy" my-telepathy-c-style)
