# Migration from Irregular Naming

Earlier internal builds used the term `irregular` for this feature. The public
plugin uses `scatter region` terminology because the generator creates
non-grid, randomly distributed regions that contrast with regular city blocks.

Recommended name changes:

| Old concept | Public concept |
| --- | --- |
| Irregular region | Scatter region |
| `generate_irregular_region` | `generate_scatter_region` |
| Irregular recipe asset | Scatter region recipe asset |
| Irregular props | Scatter props |

The first public version keeps the generated layout behavior equivalent, but
uses the new API names for the plugin surface.
