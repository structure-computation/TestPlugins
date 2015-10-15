#
class FieldSetDataDirectory extends TreeItem
    constructor: (name = 'directory' ) ->
        super()
        
        @_name.set name

        field_set_data = new FieldSetDataCorreliModel

        @add_attr
            _file_field_set_data   : new  File (@_name.get() + "_field_data"), field_set_data
   
   
#     accept_child: ( ch ) ->
#         ch instanceof FileItem