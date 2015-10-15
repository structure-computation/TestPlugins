#
class CorrelationItem extends TreeItem_Computable
    constructor: ->
        super()
        
        # default values
        @_name.set "Correlation"
        @_ico.set "img/correlation_19.png"
        @_viewable.set false

        @add_child new DiscretizationItem
        
        img_field_set_item = new ImgFieldSetDirectoryItem
        @add_child img_field_set_item


        @add_output new FieldSetCorreliItem img_field_set_item
        
        # attributes
        @add_attr
            parameters   :
                rigid_body      : true
                lum_corr        : true
                uncertainty     : false
                norm_inf        : 1e-2 # [ new Choice( 0, [ "||dU||2", "||dU||inf"] ), 1e-5 ]
                norm_2          : 0    # [ new Choice( 0, [ "||dU||2", "||dU||inf"] ), 1e-5 ]

                multi_res       : new ConstrainedVal( 0, { min: 0, max: 10, div: 10 } )
                nb_iter_max     : 100
                lambda_bulk     : 0

            
    accept_child: ( ch ) ->
        ch instanceof DiscretizationItem or
        ch instanceof SketchItem or 
        ch instanceof TransformItem or
        ch instanceof PhysicsItem or
        ch instanceof BoundariesSelectionItem
        ch instanceof ImgFieldSetDirectoryItem
        
    cosmetic_attribute: ( name ) ->
       super( name ) or ( name in [ "_output","visualization", "_residual", "_residual_adv", "_residual_int", "_residual_int_adv", "_norm_i_history", "_norm_2_history", "_residual_history" ] )
        