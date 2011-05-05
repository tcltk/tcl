function variableTk () {
   tcl("::oo::" + $(this).attr("id"), 'changed', $(this).val());
}

function buttonTk () {
   tcl("::oo::" + $(this).attr("id"), 'changed');
}

function rbuttonTk () {
   tcl("::oo::" + $(this).attr("id"), 'changed', $(this).val());
}

function cbuttonTk () {
   var val = this.value;
   if(!$(this).is(":checked")) {
      val = 0;
   }
   tcl("::oo::" + $(this).attr("id"), 'changed', val);
}

function sliderTk (e, id, ui) {
   if(e.originalEvent!=undefined) {
      tcl("::oo::" + id, 'changed', ui.value);
   }
}

function autocompleteTk (event,entry) {
   if(event.originalEvent!=undefined) {
        tcl("::oo::" + $(entry).attr("id"), 'changed', $(entry).val());
   }
}
